import zenoh
import time
import json

KEY_EXPR = "vanetza/out/cam"

def listener(sample):
    """
    Callback function triggered when data is received.
    """
    print(f"\n[Received] Key: {sample.key_expr}")
    # print(f"{sample.payload}")
    
    try:
        # payload is bytes, decode to utf-8 string
        payload_str = str(sample.payload)
        
        # Parse JSON to verify structure and pretty print
        json_obj = json.loads(payload_str)
        print("Content (JSON):")
        print(json.dumps(json_obj, indent=4))
        
    except json.JSONDecodeError:
        print("Content (Raw - Not valid JSON):")
        print(sample.payload.decode('utf-8'))
    except Exception as e:
        print(f"Error processing message: {e}")

def main():
    # 1. Initialize Zenoh Session
    print("Opening Zenoh session...")
    conf = zenoh.Config()

    # Set mode to 'client' (lightweight, connects to a router/peer)
    # conf.insert("mode", "client")

    # 1. DISABLE automatic discovery (Scouting)
    conf.insert_json5("scouting/multicast/enabled", "false")

    # Connect explicitly to the Docker instance via TCP
    # Ensure port 7447 is exposed in your Docker container
    conf.insert_json5("connect/endpoints", '["tcp/192.168.98.10:7447"]')

    with zenoh.open(conf) as session:
        
        # 2. Declare Subscriber
        print(f"Subscribing to key: '{KEY_EXPR}'")
        print("Waiting for messages... (Press CTRL+C to quit)")
        
        # Declare the subscriber with the listener callback
        sub = session.declare_subscriber(KEY_EXPR, listener)
        
        # 3. Keep the script running
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("\nSubscriber stopped.")

if __name__ == "__main__":
    main()
