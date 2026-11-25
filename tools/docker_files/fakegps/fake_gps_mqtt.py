#!/usr/bin/env python3
"""
Fake GPS fed by MQTT
====================
Subscribes to an MQTT broker (no auth, no TLS) and receives vehicle position
messages as JSON (see cithrus_vehicle_position.json). Each position is
converted to NMEA sentences (GGA, RMC, VTG, GSA) and streamed over a virtual
serial port (PTY), so gpsd can read it like a real GPS USB device.

The last known position is re-emitted every --interval seconds (with a fresh
timestamp) so gpsd keeps a fix even if MQTT messages arrive irregularly.
If no message is received for --timeout seconds, the fix is reported as lost.

Usage:
    python3 fake_gps_mqtt.py --symlink /dev/ttyGPS
    gpsd -N -n /dev/ttyGPS

Requirements:
    - Linux (PTY)
    - paho-mqtt  (pip install paho-mqtt  /  apt install python3-paho-mqtt)

Expected JSON:
    {
      "timestamp": "2026-11-24T12:51:14.777907Z",
      "vehicle": {
        "currentLocation": {"latitude": .., "longitude": .., "altitude": ..},
        "currentRotation": {"roll": .., "pitch": .., "yaw": ..},
        "linearVelocity":  {"lateral": .., "longitudinal": .., "vertical": ..},
        ...
      }
    }
"""

import os
import sys
import pty
import tty
import json
import math
import time
import signal
import argparse
import threading
from datetime import datetime, timezone

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("paho-mqtt is required: pip install paho-mqtt (or apt install python3-paho-mqtt)",
          file=sys.stderr)
    sys.exit(1)


MS_TO_KNOTS = 1.943844492
MS_TO_KMH = 3.6


# ── logging ──────────────────────────────────────────────────────────────────
class C:
    GREEN  = "\033[92m"
    YELLOW = "\033[93m"
    CYAN   = "\033[96m"
    RED    = "\033[91m"
    BOLD   = "\033[1m"
    DIM    = "\033[2m"
    RESET  = "\033[0m"


def log(msg: str, colour: str = C.RESET) -> None:
    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"{C.DIM}[{ts}]{C.RESET} {colour}{msg}{C.RESET}", flush=True)


# ── NMEA helpers ─────────────────────────────────────────────────────────────
def nmea_checksum(body: str) -> str:
    chk = 0
    for ch in body:
        chk ^= ord(ch)
    return f"{chk:02X}"


def nmea(body: str) -> str:
    """Wrap a sentence body (without $ / *XX) into a full NMEA sentence."""
    return f"${body}*{nmea_checksum(body)}"


def fmt_lat(lat: float) -> tuple[str, str]:
    hemi = "N" if lat >= 0 else "S"
    lat = abs(lat)
    deg = int(lat)
    minutes = (lat - deg) * 60.0
    return f"{deg:02d}{minutes:010.7f}", hemi


def fmt_lon(lon: float) -> tuple[str, str]:
    hemi = "E" if lon >= 0 else "W"
    lon = abs(lon)
    deg = int(lon)
    minutes = (lon - deg) * 60.0
    return f"{deg:03d}{minutes:010.7f}", hemi


def build_sentences(lat: float, lon: float, alt: float, speed_ms: float,
                    course: float, now: datetime, valid: bool,
                    talker: str = "GP") -> list[str]:
    hhmmss = now.strftime("%H%M%S.") + f"{now.microsecond // 10000:02d}"
    ddmmyy = now.strftime("%d%m%y")
    lat_s, lat_h = fmt_lat(lat)
    lon_s, lon_h = fmt_lon(lon)
    knots = speed_ms * MS_TO_KNOTS
    kmh = speed_ms * MS_TO_KMH
    course %= 360.0

    fix_q = "1" if valid else "0"
    status = "A" if valid else "V"
    mode = "A" if valid else "N"
    fix_type = "3" if valid else "1"

    gga = (f"{talker}GGA,{hhmmss},{lat_s},{lat_h},{lon_s},{lon_h},"
           f"{fix_q},12,0.8,{alt:.2f},M,0.0,M,,")
    rmc = (f"{talker}RMC,{hhmmss},{status},{lat_s},{lat_h},{lon_s},{lon_h},"
           f"{knots:.3f},{course:.2f},{ddmmyy},,,{mode}")
    vtg = f"{talker}VTG,{course:.2f},T,,M,{knots:.3f},N,{kmh:.3f},K,{mode}"
    gsa = f"{talker}GSA,A,{fix_type},01,02,03,04,05,06,07,08,09,10,11,12,1.2,0.8,0.9"
    return [nmea(s) for s in (gga, rmc, vtg, gsa)]


# ── JSON parsing ─────────────────────────────────────────────────────────────
def parse_position(payload: bytes, yaw_offset: float, yaw_ccw: bool) -> dict:
    data = json.loads(payload)
    veh = data["vehicle"]
    loc = veh["currentLocation"]
    rot = veh.get("currentRotation", {}) or {}
    vel = veh.get("linearVelocity", {}) or {}

    lat = float(loc["latitude"])
    lon = float(loc["longitude"])
    alt = float(loc.get("altitude", 0.0) or 0.0)
    if not (-90.0 <= lat <= 90.0 and -180.0 <= lon <= 180.0):
        raise ValueError(f"lat/lon out of range: {lat}, {lon}")

    vx = float(vel.get("longitudinal", 0.0) or 0.0)
    vy = float(vel.get("lateral", 0.0) or 0.0)
    speed = math.hypot(vx, vy)

    yaw = float(rot.get("yaw", 0.0) or 0.0)
    course = (-yaw if yaw_ccw else yaw) + yaw_offset

    return {"lat": lat, "lon": lon, "alt": alt, "speed": speed,
            "course": course % 360.0, "src_ts": data.get("timestamp")}


# ── Fake GPS ─────────────────────────────────────────────────────────────────
class MqttFakeGPS:
    def __init__(self, args: argparse.Namespace):
        self.args = args
        self.running = False
        self._lock = threading.Lock()
        self._pos: dict | None = None
        self._last_rx = 0.0
        self._rx_count = 0
        self._new_data = threading.Event()
        self._master_fd = -1
        self._slave_fd = -1
        self._slave_name = ""
        self._client: mqtt.Client | None = None

    # ── PTY ──────────────────────────────────────────────────────────────────
    def _open_pty(self) -> None:
        self._master_fd, self._slave_fd = pty.openpty()
        self._slave_name = os.ttyname(self._slave_fd)
        tty.setraw(self._master_fd)
        link = self.args.symlink
        if os.path.islink(link) or os.path.exists(link):
            os.remove(link)
        os.symlink(self._slave_name, link)

    def _close_pty(self) -> None:
        for fd in (self._slave_fd, self._master_fd):
            try:
                os.close(fd)
            except OSError:
                pass
        if os.path.islink(self.args.symlink):
            os.remove(self.args.symlink)

    def _send(self, sentence: str) -> None:
        try:
            os.write(self._master_fd, (sentence + "\r\n").encode("ascii"))
        except OSError as exc:
            if self.args.verbose:
                log(f"Write error (reader gone?): {exc}", C.YELLOW)

    # ── MQTT ─────────────────────────────────────────────────────────────────
    def _on_connect(self, client, userdata, flags, rc, properties=None):
        rc_val = getattr(rc, "value", rc)
        if rc_val == 0:
            log(f"MQTT connected to {self.args.host}:{self.args.port}, "
                f"subscribing to {self.args.topic!r}", C.GREEN)
            client.subscribe(self.args.topic, qos=self.args.qos)
        else:
            log(f"MQTT connect failed: rc={rc}", C.RED)

    def _on_disconnect(self, client, userdata, *args):
        if self.running:
            log("MQTT disconnected, will reconnect …", C.YELLOW)

    def _on_message(self, client, userdata, msg):
        try:
            pos = parse_position(msg.payload, self.args.yaw_offset, self.args.yaw_ccw)
        except (ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
            log(f"Bad message on {msg.topic}: {exc}", C.YELLOW)
            return
        with self._lock:
            self._pos = pos
            self._last_rx = time.monotonic()
            self._rx_count += 1
        if self.args.verbose:
            log(f"← lat={pos['lat']:.7f} lon={pos['lon']:.7f} alt={pos['alt']:.1f} "
                f"spd={pos['speed']:.2f}m/s crs={pos['course']:.1f}", C.CYAN)
        if self.args.on_message:
            self._new_data.set()

    def _make_client(self) -> "mqtt.Client":
        client_id = self.args.client_id or f"fake-gps-{os.getpid()}"
        if hasattr(mqtt, "CallbackAPIVersion"):  # paho-mqtt >= 2.0
            client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
        else:
            client = mqtt.Client(client_id=client_id)
        client.on_connect = self._on_connect
        client.on_disconnect = self._on_disconnect
        client.on_message = self._on_message
        client.reconnect_delay_set(min_delay=1, max_delay=10)
        return client

    # ── streaming ────────────────────────────────────────────────────────────
    def _emit(self) -> None:
        with self._lock:
            pos = self._pos
            age = time.monotonic() - self._last_rx
            count = self._rx_count
        if pos is None:
            return  # nothing received yet: stay silent, gpsd waits
        valid = age <= self.args.timeout
        now = datetime.now(timezone.utc)
        for s in build_sentences(pos["lat"], pos["lon"], pos["alt"], pos["speed"],
                                 pos["course"], now, valid, self.args.talker):
            self._send(s)
            if self.args.verbose:
                log(f"→ {s}", C.GREEN)
        if not self.args.verbose:
            state = f"{C.GREEN}FIX" if valid else f"{C.RED}NO FIX (stale {age:.0f}s)"
            print(f"\r{state}{C.RESET} lat={pos['lat']:.7f} lon={pos['lon']:.7f} "
                  f"spd={pos['speed']:.2f}m/s crs={pos['course']:.1f} rx={count}   ",
                  end="", flush=True)

    def _stream_loop(self) -> None:
        while self.running:
            if self.args.on_message:
                # emit on every new message, but at least every interval
                self._new_data.wait(timeout=self.args.interval)
                self._new_data.clear()
            else:
                time.sleep(self.args.interval)
            if self.running:
                self._emit()

    # ── public API ───────────────────────────────────────────────────────────
    def start(self) -> None:
        self._open_pty()
        self.running = True

        print()
        print(f"{C.BOLD}{C.GREEN}✓ Fake GPS (MQTT) device ready{C.RESET}")
        print(f"  PTY device : {C.BOLD}{self._slave_name}{C.RESET}")
        print(f"  Symlink    : {C.BOLD}{self.args.symlink}{C.RESET}  ← point gpsd here")
        print(f"  Broker     : {self.args.host}:{self.args.port}")
        print(f"  Topic      : {self.args.topic}")
        print(f"  Interval   : {self.args.interval}s   timeout: {self.args.timeout}s")
        print(f"\n  Connect with:  gpsd -N -n {self.args.symlink}\n", flush=True)

        self._client = self._make_client()
        # connect_async + loop_start: keeps retrying if the broker isn't up yet
        self._client.connect_async(self.args.host, self.args.port, keepalive=30)
        self._client.loop_start()

        self._stream_loop()

    def stop(self) -> None:
        self.running = False
        self._new_data.set()
        if self._client is not None:
            self._client.loop_stop()
            self._client.disconnect()
        self._close_pty()
        print()
        log("Fake GPS stopped.", C.CYAN)


# ── CLI ──────────────────────────────────────────────────────────────────────
def main() -> None:
    p = argparse.ArgumentParser(
        description="Fake GPS device fed by MQTT JSON vehicle positions, "
                    "exposed as NMEA over a virtual serial port (PTY) for gpsd.")
    p.add_argument("--host", default="localhost", help="MQTT broker host (default: localhost)")
    p.add_argument("--port", type=int, default=1883, help="MQTT broker port (default: 1883)")
    p.add_argument("--topic", default="vanetza/gnss",
                   help="MQTT topic (default: vanetza/gnss)")
    p.add_argument("--qos", type=int, default=0, choices=(0, 1, 2), help="Subscribe QoS")
    p.add_argument("--client-id", default=None, help="MQTT client id (default: fake-gps-<pid>)")
    p.add_argument("--symlink", default="/tmp/fake_gps",
                   help="Symlink path for the virtual port (default: /tmp/fake_gps)")
    p.add_argument("--interval", type=float, default=1.0,
                   help="Seconds between NMEA epochs (default: 1.0)")
    p.add_argument("--on-message", action="store_true",
                   help="Also emit NMEA immediately on every MQTT message")
    p.add_argument("--timeout", type=float, default=5.0,
                   help="Seconds without MQTT data before reporting no fix (default: 5)")
    p.add_argument("--yaw-offset", type=float, default=0.0,
                   help="Degrees added to yaw to get course over ground (default: 0)")
    p.add_argument("--yaw-ccw", action="store_true",
                   help="Yaw is counter-clockwise (ENU style); negate before applying offset. "
                        "For ENU yaw (0=east, CCW) use --yaw-ccw --yaw-offset 90")
    p.add_argument("--talker", default="GP", help="NMEA talker id (default: GP)")
    p.add_argument("--verbose", action="store_true", help="Print every message/sentence")
    args = p.parse_args()

    gps = MqttFakeGPS(args)

    def _handle_signal(sig, frame):
        print()
        log("Caught signal, shutting down …", C.YELLOW)
        gps.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, _handle_signal)
    signal.signal(signal.SIGTERM, _handle_signal)

    gps.start()


if __name__ == "__main__":
    main()
