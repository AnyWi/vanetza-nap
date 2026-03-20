#!/usr/bin/env python3
"""
Fake GPS USB/TTY NMEA Sentence Simulator
=========================================
Creates a virtual serial port (PTY) and streams NMEA sentences from a file,
simulating a real GPS USB device.

Usage:
    python fake_gps.py [nmea_file] [options]

Requirements:
    - Linux (uses PTY - pseudo-terminal)
    - pyserial (optional, for connecting clients)

Quick start:
    1. Run: python fake_gps.py gps_data.nmea
    2. Note the symlink path printed (e.g. /tmp/fake_gps)
    3. Point your GPS client to that port at 4800 baud
"""

import os
import sys
import pty
import tty
import time
import signal
import select
import argparse
import threading
from pathlib import Path
from datetime import datetime


# ── ANSI colours ─────────────────────────────────────────────────────────────
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
    print(f"{C.DIM}[{ts}]{C.RESET} {colour}{msg}{C.RESET}")

def log_overwrite(msg: str, colour: str = C.RESET) -> None:
    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"{C.DIM}[{ts}]{C.RESET} {colour}{msg}{C.RESET}", end='\r', flush=True)


# ── NMEA helpers ──────────────────────────────────────────────────────────────
def nmea_checksum(sentence: str) -> str:
    """Compute XOR checksum for an NMEA sentence (without $ and *XX)."""
    chk = 0
    for ch in sentence:
        chk ^= ord(ch)
    return f"{chk:02X}"


def validate_sentence(line: str) -> tuple[bool, str]:
    """
    Validate an NMEA sentence.
    Returns (is_valid, reason).
    """
    line = line.strip()
    if not line:
        return False, "empty"
    if not line.startswith("$"):
        return False, "no leading $"
    if "*" in line:
        body, cs = line[1:].rsplit("*", 1)
        expected = nmea_checksum(body)
        if cs.upper() != expected.upper():
            return False, f"checksum mismatch (got {cs.upper()}, expected {expected})"
    return True, "ok"


def rewrite_timestamp(sentence: str, update_time: bool) -> str:
    """
    Optionally rewrite the time field of GGA/RMC/GLL sentences to 'now'.
    """
    if not update_time:
        return sentence
    parts = sentence.strip().split(",")
    talker = parts[0]  # e.g. $GPGGA
    if talker in ("$GPGGA", "$GNGGA", "$GPGLL", "$GNGLL"):
        time_idx = 1
    elif talker in ("$GPRMC", "$GNRMC"):
        time_idx = 1
    else:
        return sentence

    now = datetime.utcnow()
    new_time = now.strftime("%H%M%S.") + f"{now.microsecond // 1000:03d}"
    parts[time_idx] = new_time

    # Recompute checksum
    if "*" in parts[-1]:
        parts[-1] = parts[-1].split("*")[0]
        body = ",".join(parts)[1:]          # strip leading $
        parts[-1] = parts[-1] + "*" + nmea_checksum(body)

    return ",".join(parts)


# ── File loader ───────────────────────────────────────────────────────────────
def load_nmea_file(path: str) -> list[str]:
    """Load NMEA sentences from file, skipping blanks and comments."""
    sentences = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            valid, reason = validate_sentence(line)
            if valid:
                sentences.append(line)
            else:
                log(f"Line {lineno} skipped ({reason}): {line!r}", C.YELLOW)
    return sentences


# ── PTY / virtual serial port ─────────────────────────────────────────────────
class FakeGPS:
    def __init__(
        self,
        nmea_file: str,
        symlink: str = "/tmp/fake_gps",
        baud: int = 4800,
        interval: float = 1.0,
        loop: bool = True,
        update_time: bool = False,
        verbose: bool = False,
    ):
        self.nmea_file   = nmea_file
        self.symlink     = symlink
        self.baud        = baud
        self.interval    = interval
        self.loop        = loop
        self.update_time = update_time
        self.verbose     = verbose

        self.sentences: list[str] = []
        self.running = False
        self._master_fd: int = -1
        self._slave_fd:  int = -1
        self._slave_name: str = ""

    # ── setup / teardown ──────────────────────────────────────────────────────
    def _open_pty(self) -> None:
        self._master_fd, self._slave_fd = pty.openpty()
        self._slave_name = os.ttyname(self._slave_fd)

        # Configure raw mode on master so writes go straight through
        tty.setraw(self._master_fd)

        # Create convenience symlink
        if os.path.islink(self.symlink) or os.path.exists(self.symlink):
            os.remove(self.symlink)
        os.symlink(self._slave_name, self.symlink)

    def _close_pty(self) -> None:
        for fd in (self._slave_fd, self._master_fd):
            try:
                os.close(fd)
            except OSError:
                pass
        if os.path.islink(self.symlink):
            os.remove(self.symlink)

    # ── streaming ─────────────────────────────────────────────────────────────
    def _send(self, sentence: str) -> None:
        """Write one NMEA sentence (with CRLF) to the PTY master."""
        line = sentence + "\r\n"
        data = line.encode("ascii", errors="replace")
        try:
            os.write(self._master_fd, data)
        except OSError as exc:
            # Reader disconnected – not fatal
            if self.verbose:
                log(f"Write error (reader gone?): {exc}", C.YELLOW)

    def _stream_loop(self) -> None:
        idx = 0
        total = len(self.sentences)
        run = True

        while run and self.running:
            sentence = self.sentences[idx]
            sentence = rewrite_timestamp(sentence, self.update_time)
            self._send(sentence)

            if self.verbose:
                log(f"→ {sentence}", C.GREEN)
            else:
                # Show sentence type only
                tag = sentence.split(",")[0]
                log_overwrite(f"→ Progress: {((idx+1)/total*100):.2f}% -- {tag} ({idx+1}/{total})", C.GREEN)

            idx += 1
            if idx >= total:
                if self.loop:
                    idx = 0
                    log("↺  Looping back to start of file", C.CYAN)
                else:
                    run = False

            # Honour interval between sentences
            deadline = time.monotonic() + self.interval
            while time.monotonic() < deadline and self.running:
                time.sleep(0.05)

        log("Stream finished.", C.CYAN)
        self.running = False

    # ── public API ────────────────────────────────────────────────────────────
    def start(self) -> None:
        log(f"Loading NMEA data from {self.nmea_file!r} …", C.CYAN)
        self.sentences = load_nmea_file(self.nmea_file)
        if not self.sentences:
            log("No valid NMEA sentences found. Exiting.", C.RED)
            sys.exit(1)
        log(f"Loaded {len(self.sentences)} sentence(s).", C.GREEN)

        self._open_pty()
        self.running = True

        print()
        print(f"{C.BOLD}{C.GREEN}✓ Fake GPS device ready{C.RESET}")
        print(f"  PTY device : {C.BOLD}{self._slave_name}{C.RESET}")
        print(f"  Symlink    : {C.BOLD}{self.symlink}{C.RESET}  ← point your client here")
        print(f"  Baud rate  : {self.baud}")
        print(f"  Interval   : {self.interval}s per sentence")
        print(f"  Loop       : {self.loop}")
        print()
        print(f"  Connect with:")
        print(f"    gpsd -N -n {self.symlink}")
        print(f"    minicom -D {self.symlink} -b {self.baud}")
        print(f"    cat {self.symlink}")
        print()
        print(f"  Press Ctrl+C to stop.")
        print()

        # Stream in background thread
        t = threading.Thread(target=self._stream_loop, daemon=True)
        t.start()
        t.join()

    def stop(self) -> None:
        self.running = False
        self._close_pty()
        log("Fake GPS stopped.", C.CYAN)


# ── signal handler ────────────────────────────────────────────────────────────
_gps_instance: "FakeGPS | None" = None

def _handle_signal(sig, frame):
    print()
    log("Caught signal, shutting down …", C.YELLOW)
    if _gps_instance:
        _gps_instance.stop()
    sys.exit(0)


# ── sample data generator ─────────────────────────────────────────────────────
SAMPLE_DATA = """\
# Sample NMEA data – generated by fake_gps.py
$GPGGA,120849.114,5221.832,N,00453.294,E,1,12,1.0,0.0,M,0.0,M,,*6C
$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*30
$GPRMC,120849.114,A,5221.832,N,00453.294,E,038.9,190.6,170226,000.0,W*74
$GPGGA,120850.114,5221.821,N,00453.291,E,1,12,1.0,0.0,M,0.0,M,,*63
$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*30
$GPRMC,120850.114,A,5221.821,N,00453.291,E,038.9,190.6,170226,000.0,W*7B
$GPGGA,120851.114,5221.810,N,00453.287,E,1,12,1.0,0.0,M,0.0,M,,*67
$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*30
$GPRMC,120851.114,A,5221.810,N,00453.287,E,038.9,190.6,170226,000.0,W*7D
$GPGGA,120852.114,5221.799,N,00453.284,E,1,12,1.0,0.0,M,0.0,M,,*68
$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*30
$GPRMC,120852.114,A,5221.799,N,00453.284,E,038.9,190.6,170226,000.0,W*72
"""


# ── CLI ───────────────────────────────────────────────────────────────────────
def main() -> None:
    parser = argparse.ArgumentParser(
        description="Fake GPS device — streams NMEA sentences over a virtual serial port (PTY).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Use bundled sample data
  python fake_gps.py

  # Use your own NMEA file
  python fake_gps.py source.nmea

  # Custom symlink, 1Hz, no loop
  python fake_gps.py source.nmea --symlink /dev/ttyGPS --interval 1.0 --no-loop

  # Rewrite timestamps to current UTC time
  python fake_gps.py source.nmea --update-time

  # Verbose: print every sentence
  python fake_gps.py source.nmea --verbose
        """,
    )
    parser.add_argument(
        "nmea_file",
        nargs="?",
        default=None,
        help="Path to NMEA file (default: use built-in sample data)",
    )
    parser.add_argument(
        "--symlink",
        default="/tmp/fake_gps",
        help="Symlink path for the virtual port (default: /tmp/fake_gps)",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=4800,
        help="Reported baud rate (informational, default: 4800)",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=1.0,
        help="Seconds between sentences (default: 1.0)",
    )
    parser.add_argument(
        "--no-loop",
        action="store_true",
        help="Stop after playing through the file once",
    )
    parser.add_argument(
        "--update-time",
        action="store_true",
        help="Rewrite GGA/RMC time fields to current UTC",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print full sentence on each write",
    )
    parser.add_argument(
        "--create-sample",
        metavar="PATH",
        help="Write sample NMEA file to PATH and exit",
    )

    args = parser.parse_args()

    # --create-sample shortcut
    if args.create_sample:
        Path(args.create_sample).write_text(SAMPLE_DATA)
        print(f"Sample NMEA data written to {args.create_sample!r}")
        sys.exit(0)

    # Resolve file: use temp sample if not provided
    if args.nmea_file is None:
        sample_path = "/tmp/_fake_gps_sample.nmea"
        Path(sample_path).write_text(SAMPLE_DATA)
        nmea_file = sample_path
        log(f"No file given — using built-in sample data ({sample_path})", C.YELLOW)
    else:
        nmea_file = args.nmea_file
        if not os.path.exists(nmea_file):
            log(f"File not found: {nmea_file!r}", C.RED)
            sys.exit(1)

    global _gps_instance
    _gps_instance = FakeGPS(
        nmea_file   = nmea_file,
        symlink     = args.symlink,
        baud        = args.baud,
        interval    = args.interval,
        loop        = not args.no_loop,
        update_time = args.update_time,
        verbose     = args.verbose,
    )

    signal.signal(signal.SIGINT,  _handle_signal)
    signal.signal(signal.SIGTERM, _handle_signal)

    _gps_instance.start()


if __name__ == "__main__":
    main()