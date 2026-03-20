import zenoh
import time
import json
import os

KEY_EXPR = "vanetza/in/cam"
FILENAME = "../in_cam.json"

#cam_hex_dump = "40 59 bb 89 bc ac c4 28 7c 3f ff ff fc 23 b7 74 3e 50 5f af c1 b2 fe bf e9 ea 73 37 fe ea 11 2a 00 6a 9f 80 00"


def main():
    # 1. Initialize Zenoh Session
    print(f"Opening Zenoh session...")
    conf = zenoh.Config()

    # Set mode to 'client' (lightweight, connects to a router/peer)
    # conf.insert("mode", "client")

    # 1. DISABLE automatic discovery (Scouting)
    conf.insert_json5("scouting/multicast/enabled", "false")

    # Connect explicitly to the Docker instance via TCP
    # Ensure port 7447 is exposed in your Docker container
    conf.insert_json5("connect/endpoints", '["tcp/192.168.98.20:7447"]') # .20 -> obu .10 -> rsu
    
    # Open the session using the 'with' syntax for auto-cleanup
    with zenoh.open(conf) as session:
        
        # 2. Check if file exists
        if not os.path.exists(FILENAME):
            print(f"Error: File {FILENAME} not found.")
            return

        # 3. Read the JSON file
        try:
            with open(FILENAME, 'r') as f:
                # We read it into a generic object to ensure it is valid JSON, 
                # then dump it back to string for sending.
                data = json.load(f)
                payload = json.dumps(data)
        except json.JSONDecodeError as e:
            print(f"Error decoding JSON: {e}")
            return

        print(f"Publishing to key: '{KEY_EXPR}'")
        print(f"Payload: {payload}")

        # 4. Publish the data
        # Zenoh put() handles string encoding automatically
        session.put(KEY_EXPR, payload)
        
        print("Message sent successfully.")
        
        # Short sleep to ensure network buffer flush if terminating immediately
        time.sleep(1)

        # # 5. Publish binary data
        # print("Sending binary(asn.1?) CAM")

        # payload_bin = bytes.fromhex(cam_hex_dump)

        # session.put(KEY_EXPR, payload_bin)

        # time.sleep(1)

if __name__ == "__main__":
    main()
