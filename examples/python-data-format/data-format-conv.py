import json
import sys
import paho.mqtt.publish as publish

TOPIC_NAME="vanetza/in/cpm"
BROKER_HOST="192.168.98.10"

def translate_to_etsi_cpm(input_objects):
    """
    Translates a list of custom detected objects into an ETSI-compliant CPM JSON.
    """
    if not input_objects:
        return "{}"

    # Extract reference time from the first object's timestamp (ms since 2004 epoch)
    reference_time = input_objects[0].get("timestamp", 0)

    # Base ETSI CPM Structure
    cpm_message = {
        "managementContainer": {
            "referenceTime": reference_time,
            "referencePosition": {
                "altitude": {"altitudeConfidence": 15, "altitudeValue": 2.9},
                "latitude": 50.63481,
                "longitude": -20.660463,
                "positionConfidenceEllipse": {
                    "semiMajorConfidence": 4095,
                    "semiMajorOrientation": 0,
                    "semiMinorConfidence": 4095
                }
            }
        },
        "cpmContainers": [
            {
                "containerId": 1,
                "containerData": {
                    "orientationAngle": {"value": 0, "confidence": 1}
                }
            },
            {
                "containerId": 3,
                "containerData": [
                    {
                        "sensorId": 1,
                        "sensorType": 1,
                        "description": "Primary Fusion Sensor",
                        "perceptionRegionShape": {
                            "radial": {
                                "range": 150,
                                "horizontalOpeningAngleStart": 3601,
                                "horizontalOpeningAngleEnd": 3601
                            }
                        },
                        "shadowingApplies": True
                    }
                ]
            },
            {
                "containerId": 5,
                "containerData": {
                    "numberOfPerceivedObjects": len(input_objects),
                    "perceivedObjects": []
                }
            }
        ]
    }

    perceived_objects_list = []

    # Map each input object to the Perceived Object Container
    for obj in input_objects:
        obj_type = obj.get("Object_type", 1)
        obj_id = obj.get("Object_ID", 0)
        pos = obj.get("position", [0.0, 0.0])
        vel = obj.get("velocity", [0.0, 0.0])
        conf = obj.get("confidence", 0.0)
        
        confidence_percent = int(conf*100)

        # Map Object Type (1 = Pedestrian, Others = Vehicle)
        if obj_type == 1:
            classification = [{"objectClass": {"vruSubClass": {"pedestrian": 1}}, "confidence": confidence_percent}]
            desc = "Pedestrian"
        else:
            classification = [{"objectClass": {"vehicleSubClass": 1}, "confidence": confidence_percent}]
            desc = "Vehicle"

        # Construct Position dynamically (Z is optional)
        position = {
            "xCoordinate": {"value": pos[0], "confidence": 1},
            "yCoordinate": {"value": pos[1], "confidence": 1}
        }
        if len(pos) >= 3:
            position["zCoordinate"] = {"value": pos[2], "confidence": 1}

        # Construct v dynamically (Z is optional)
        cartesian_velocity = {
            "xVelocity": {"value": vel[0], "confidence": 1},
            "yVelocity": {"value": vel[1], "confidence": 1}
        }
        if len(vel) >= 3:
            cartesian_velocity["zVelocity"] = {"value": vel[2], "confidence": 1}

        # Construct the ETSI perceived object
        etsi_obj = {
            "objectId": obj_id,
            "sensorIdList": [1], 
            "measurementDeltaTime": 10, # Note: statically set to 10 right now
            "position": position,
            "velocity": {
                "cartesianVelocity": cartesian_velocity
            },
            "classification": classification,
            "description": desc
        }
        perceived_objects_list.append(etsi_obj)

    # Attach the objects array to Container 5
    cpm_message["cpmContainers"][2]["containerData"]["perceivedObjects"] = perceived_objects_list

    return json.dumps(cpm_message, indent=4)

def mqtt_publish(input_str):
    publish.single(TOPIC_NAME, input_str, hostname=BROKER_HOST)

# --- Example Usage ---
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Error in arguments")
        sys.exit(1)
    if ".json" in sys.argv[1]:
        fpath = sys.argv[1]
        with open(fpath, 'r') as f:
            try:
                input = json.load(f)
                # print(translate_to_etsi_cpm(input))
                mqtt_publish(translate_to_etsi_cpm(input))
            except Exception as e:
                print(e)
    
