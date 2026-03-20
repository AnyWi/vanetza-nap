#include "config.hpp"

message_config_t read_message_config(INIReader reader, string env_prefix, string ini_section) {
    message_config_t res = {
        getenv((env_prefix + "_ENABLED").c_str()) == NULL ? reader.GetBoolean(ini_section, "enabled", true) : (strcmp(getenv((env_prefix + "_ENABLED").c_str()), "true") == 0),
        getenv((env_prefix + "_PERIODICITY").c_str()) == NULL ? reader.GetInteger(ini_section, "periodicity", 0) : stoi(getenv((env_prefix + "_PERIODICITY").c_str())),
        getenv((env_prefix + "_TOPIC_IN").c_str()) == NULL ? reader.Get(ini_section, "topic_in", "target/none") : getenv((env_prefix + "_TOPIC_IN").c_str()),
        getenv((env_prefix + "_TOPIC_OUT").c_str()) == NULL ? reader.Get(ini_section, "topic_out", "vanetza/message") : getenv((env_prefix + "_TOPIC_OUT").c_str()),
        getenv((env_prefix + "_TOPIC_TIME").c_str()) == NULL ? reader.Get(ini_section, "topic_time", "vanetza/time") : getenv((env_prefix + "_TOPIC_TIME").c_str()),
        getenv((env_prefix + "_TOPIC_TEST").c_str()) == NULL ? reader.Get(ini_section, "topic_test", "vanetza/test") : getenv((env_prefix + "_TOPIC_TEST").c_str()),
        getenv((env_prefix + "_UDP_OUT_ADDR").c_str()) == NULL ? reader.Get(ini_section, "udp_out_addr", "127.0.0.1") : getenv((env_prefix + "_UDP_OUT_ADDR").c_str()),
        getenv((env_prefix + "_UDP_OUT_PORT").c_str()) == NULL ? reader.GetInteger(ini_section, "udp_out_port", 0) : stoi(getenv((env_prefix + "_UDP_OUT_PORT").c_str())),
        getenv((env_prefix + "_MQTT_ENABLED").c_str()) == NULL ? reader.GetBoolean(ini_section, "mqtt_enabled", true) : (strcmp(getenv((env_prefix + "_MQTT_ENABLED").c_str()), "true") == 0),
        getenv((env_prefix + "_MQTT_TIME_ENABLED").c_str()) == NULL ? reader.GetBoolean(ini_section, "mqtt_time_enabled", true) : (strcmp(getenv((env_prefix + "_MQTT_TIME_ENABLED").c_str()), "true") == 0),
        getenv((env_prefix + "_MQTT_TEST_ENABLED").c_str()) == NULL ? reader.GetBoolean(ini_section, "mqtt_test_enabled", false) : (strcmp(getenv((env_prefix + "_MQTT_TEST_ENABLED").c_str()), "true") == 0),
        getenv((env_prefix + "_DDS_ENABLED").c_str()) == NULL ? reader.GetBoolean(ini_section, "dds_enabled", true) : (strcmp(getenv((env_prefix + "_DDS_ENABLED").c_str()), "true") == 0),
        getenv((env_prefix + "_ZENOH_ENABLED").c_str()) == NULL ? reader.GetBoolean(ini_section, "zenoh_enabled", false) : (strcmp(getenv((env_prefix + "_ZENOH_ENABLED").c_str()), "true") == 0)
    };
    return res;
}

// ---------------------------------------------------------------------------
// print_ini()
// Prints every key that read_config() reads from the INI file, grouped by
// section.  Unknown / missing keys show the hard-coded default value.
// ---------------------------------------------------------------------------
void print_ini(INIReader* reader)
{
    printf("\n===== INI FILE CONFIGURATION =====\n");

    // ── [station] ──────────────────────────────────────────────────────────
    printf("\n[station]\n");
    printf("  id                  = %ld\n",   reader->GetInteger ("station", "id",                  99));
    printf("  type                = %ld\n",   reader->GetInteger ("station", "type",                15));
    printf("  mac_address         = %s\n",    reader->Get        ("station", "mac_address",         "").c_str());
    printf("  beacons_enabled     = %s\n",    reader->GetBoolean ("station", "beacons_enabled",     true)  ? "true" : "false");
    printf("  use_hardcoded_gps   = %s\n",    reader->GetBoolean ("station", "use_hardcoded_gps",   true)  ? "true" : "false");
    printf("  latitude            = %f\n",    reader->GetReal    ("station", "latitude",            50));
    printf("  longitude           = %f\n",    reader->GetReal    ("station", "longitude",           -8));
    printf("  length              = %f\n",    reader->GetReal    ("station", "length",              10));
    printf("  width               = %f\n",    reader->GetReal    ("station", "width",               3));

    // ── [general] ──────────────────────────────────────────────────────────
    printf("\n[general]\n");
    printf("  interface               = %s\n",  reader->Get        ("general", "interface",               "wlan0").c_str());
    printf("  local_mqtt_broker       = %s\n",  reader->Get        ("general", "local_mqtt_broker",       "127.0.0.1").c_str());
    printf("  local_mqtt_port         = %ld\n", reader->GetInteger ("general", "local_mqtt_port",         1883));
    printf("  remote_mqtt_broker      = %s\n",  reader->Get        ("general", "remote_mqtt_broker",      "").c_str());
    printf("  remote_mqtt_port        = %ld\n", reader->GetInteger ("general", "remote_mqtt_port",        0));
    printf("  remote_mqtt_username    = %s\n",  reader->Get        ("general", "remote_mqtt_username",    "").c_str());
    printf("  remote_mqtt_password    = %s\n",  reader->Get        ("general", "remote_mqtt_password",    "").c_str());
    printf("  gpsd_host               = %s\n",  reader->Get        ("general", "gpsd_host",               "127.0.0.1").c_str());
    printf("  gpsd_port               = %s\n",  reader->Get        ("general", "gpsd_port",               "2947").c_str());
    printf("  prometheus_port         = %ld\n", reader->GetInteger ("general", "prometheus_port",         9100));
    printf("  rssi_enabled            = %s\n",  reader->GetBoolean ("general", "rssi_enabled",            true)  ? "true" : "false");
    printf("  mcs_enabled             = %s\n",  reader->GetBoolean ("general", "mcs_enabled",             false) ? "true" : "false");
    printf("  ignore_own_messages     = %s\n",  reader->GetBoolean ("general", "ignore_own_messages",     true)  ? "true" : "false");
    printf("  ignore_rsu_messages     = %s\n",  reader->GetBoolean ("general", "ignore_rsu_messages",     false) ? "true" : "false");
    printf("  enable_json_prints      = %s\n",  reader->GetBoolean ("general", "enable_json_prints",      true)  ? "true" : "false");
    printf("  dds_participant_name    = %s\n",  reader->Get        ("general", "dds_participant_name",    "Vanetza").c_str());
    printf("  dds_domain_id           = %ld\n", reader->GetInteger ("general", "dds_domain_id",           0));
    printf("  num_threads             = %ld\n", reader->GetInteger ("general", "num_threads",             -1));
    printf("  publish_encoded_payloads= %s\n",  reader->GetBoolean ("general", "publish_encoded_payloads",false) ? "true" : "false");
    printf("  debug_enabled           = %s\n",  reader->GetBoolean ("general", "debug_enabled",           false) ? "true" : "false");
    printf("  zenoh_local_only        = %s\n",  reader->GetBoolean ("general", "zenoh_local_only",        true)  ? "true" : "false");
    printf("  zenoh_interfaces        = %s\n",  reader->Get        ("general", "zenoh_interfaces",        "").c_str());

    // ── message sections ───────────────────────────────────────────────────
    // Each message type shares the same set of sub-keys used by
    // read_message_config(); adjust if your helper reads different keys.
    const char* msg_sections[] = {
        "cam", "denm", "cpm", "vam", "spatem", "mapem",
        "ssem", "srem", "rtcmem", "ivim", "imzm",
        "evcsnm", "evrsrm", "tistpgm", "mcm"
    };
    for (const char* sec : msg_sections) {
        printf("\n[%s]\n", sec);
        printf("  enabled        = %s\n",  reader->GetBoolean (sec, "enabled",        true)  ? "true" : "false");
        printf("  topic_in       = %s\n",  reader->Get        (sec, "topic_in",       "").c_str());
        printf("  topic_out      = %s\n",  reader->Get        (sec, "topic_out",      "").c_str());
    }

    // ── cam-specific extras ────────────────────────────────────────────────
    printf("\n[cam] (extra)\n");
    printf("  own_topic_out    = %s\n",  reader->Get("cam", "own_topic_out", "").c_str());

    printf("\n==================================\n\n");
}

// ---------------------------------------------------------------------------
// print_env()
// Prints every VANETZA_* environment variable consumed by read_config().
// Variables that are not set are shown as "<not set>".
// ---------------------------------------------------------------------------
void print_env()
{
    // Helper lambda: returns the value or a placeholder string
    auto env = [](const char* name) -> const char* {
        const char* v = getenv(name);
        return v ? v : "<not set>";
    };

    printf("\n===== ENVIRONMENT VARIABLES =====\n");

    // station
    printf("\n-- station --\n");
    printf("  VANETZA_STATION_ID          = %s\n", env("VANETZA_STATION_ID"));
    printf("  VANETZA_STATION_TYPE        = %s\n", env("VANETZA_STATION_TYPE"));
    printf("  VANETZA_MAC_ADDRESS         = %s\n", env("VANETZA_MAC_ADDRESS"));
    printf("  VANETZA_BEACONS_ENABLED     = %s\n", env("VANETZA_BEACONS_ENABLED"));
    printf("  VANETZA_USE_HARDCODED_GPS   = %s\n", env("VANETZA_USE_HARDCODED_GPS"));
    printf("  VANETZA_LATITUDE            = %s\n", env("VANETZA_LATITUDE"));
    printf("  VANETZA_LONGITUDE           = %s\n", env("VANETZA_LONGITUDE"));
    printf("  VANETZA_LENGTH              = %s\n", env("VANETZA_LENGTH"));
    printf("  VANETZA_WIDTH               = %s\n", env("VANETZA_WIDTH"));

    // general
    printf("\n-- general --\n");
    printf("  VANETZA_INTERFACE               = %s\n", env("VANETZA_INTERFACE"));
    printf("  VANETZA_LOCAL_MQTT_BROKER       = %s\n", env("VANETZA_LOCAL_MQTT_BROKER"));
    printf("  VANETZA_LOCAL_MQTT_PORT         = %s\n", env("VANETZA_LOCAL_MQTT_PORT"));
    printf("  VANETZA_REMOTE_MQTT_BROKER      = %s\n", env("VANETZA_REMOTE_MQTT_BROKER"));
    printf("  VANETZA_REMOTE_MQTT_PORT        = %s\n", env("VANETZA_REMOTE_MQTT_PORT"));
    printf("  VANETZA_REMOTE_MQTT_USERNAME    = %s\n", env("VANETZA_REMOTE_MQTT_USERNAME"));
    printf("  VANETZA_REMOTE_MQTT_PASSWORD    = %s\n", env("VANETZA_REMOTE_MQTT_PASSWORD"));
    printf("  VANETZA_GPSD_HOST               = %s\n", env("VANETZA_GPSD_HOST"));
    printf("  VANETZA_GPSD_PORT               = %s\n", env("VANETZA_GPSD_PORT"));
    printf("  VANETZA_PROMETHEUS_PORT         = %s\n", env("VANETZA_PROMETHEUS_PORT"));
    printf("  VANETZA_RSSI_ENABLED            = %s\n", env("VANETZA_RSSI_ENABLED"));
    printf("  VANETZA_MCS_ENABLED             = %s\n", env("VANETZA_MCS_ENABLED"));
    printf("  VANETZA_IGNORE_OWN_MESSAGES     = %s\n", env("VANETZA_IGNORE_OWN_MESSAGES"));
    printf("  VANETZA_IGNORE_RSU_MESSAGES     = %s\n", env("VANETZA_IGNORE_RSU_MESSAGES"));
    printf("  VANETZA_ENABLE_JSON_PRINTS      = %s\n", env("VANETZA_ENABLE_JSON_PRINTS"));
    printf("  VANETZA_DDS_PARTICIPANT_NAME    = %s\n", env("VANETZA_DDS_PARTICIPANT_NAME"));
    printf("  VANETZA_DDS_DOMAIN_ID           = %s\n", env("VANETZA_DDS_DOMAIN_ID"));
    printf("  VANETZA_NUM_THREADS             = %s\n", env("VANETZA_NUM_THREADS"));
    printf("  VANETZA_PUBLISH_ENCODED_PAYLOADS= %s\n", env("VANETZA_PUBLISH_ENCODED_PAYLOADS"));
    printf("  VANETZA_DEBUG_ENABLED           = %s\n", env("VANETZA_DEBUG_ENABLED"));
    printf("  VANETZA_ZENOH_LOCAL_ONLY        = %s\n", env("VANETZA_ZENOH_LOCAL_ONLY"));
    printf("  VANETZA_ZENOH_INTERFACES        = %s\n", env("VANETZA_ZENOH_INTERFACES"));

    // message types
    printf("\n-- message types --\n");
    const char* msg_prefixes[] = {
        "VANETZA_CAM",   "VANETZA_DENM",   "VANETZA_CPM",
        "VANETZA_VAM",   "VANETZA_SPATEM", "VANETZA_MAPEM",
        "VANETZA_SSEM",  "VANETZA_SREM",   "VANETZA_RTCMEM",
        "VANETZA_IVIM",  "VANETZA_IMZM",   "VANETZA_EVCSNM",
        "VANETZA_EVRCSM","VANETZA_TISTPGM","VANETZA_MCM"
    };
    for (const char* pfx : msg_prefixes) {
        // Compose the expected env var names used by read_message_config()
        // e.g. VANETZA_CAM_ENABLED, VANETZA_CAM_TOPIC_IN, VANETZA_CAM_TOPIC_OUT
        char buf[128];
        snprintf(buf, sizeof(buf), "%s_ENABLED",   pfx); printf("  %-40s = %s\n", buf, env(buf));
        snprintf(buf, sizeof(buf), "%s_TOPIC_IN",  pfx); printf("  %-40s = %s\n", buf, env(buf));
        snprintf(buf, sizeof(buf), "%s_TOPIC_OUT", pfx); printf("  %-40s = %s\n", buf, env(buf));
    }

    // cam-specific extras
    printf("\n-- cam extras --\n");
    printf("  VANETZA_CAM_OWN_TOPIC_OUT       = %s\n", env("VANETZA_CAM_OWN_TOPIC_OUT"));

    printf("\n=================================\n\n");
}

void read_config(config_t* config_s, string path) {
    INIReader reader(path);

    // print_ini(&reader);
    // print_env();

    config_s->station_id = getenv("VANETZA_STATION_ID") == NULL ? reader.GetInteger("station", "id", 99) : stoi(getenv("VANETZA_STATION_ID"));
    config_s->station_type = getenv("VANETZA_STATION_TYPE") == NULL ? reader.GetInteger("station", "type", 15) : stoi(getenv("VANETZA_STATION_TYPE"));
    config_s->remote_mqtt_prefix = config_s->station_type == 15 ? "p" : "obu";
    config_s->mac_address = getenv("VANETZA_MAC_ADDRESS") == NULL ? reader.Get("station", "mac_address", "") : getenv("VANETZA_MAC_ADDRESS");
    config_s->beacons_enabled = getenv("VANETZA_BEACONS_ENABLED") == NULL ? reader.GetBoolean("station", "beacons_enabled", true) : (strcmp((getenv("VANETZA_BEACONS_ENABLED")), "true") == 0);
    config_s->use_hardcoded_gps = getenv("VANETZA_USE_HARDCODED_GPS") == NULL ? reader.GetBoolean("station", "use_hardcoded_gps", true) : (strcmp((getenv("VANETZA_USE_HARDCODED_GPS")), "true") == 0);
    config_s->latitude = getenv("VANETZA_LATITUDE") == NULL ? reader.GetReal("station", "latitude", 60) : stod(getenv("VANETZA_LATITUDE"));
    config_s->longitude = getenv("VANETZA_LONGITUDE") == NULL ? reader.GetReal("station", "longitude", -16) : stod(getenv("VANETZA_LONGITUDE"));
    config_s->length = getenv("VANETZA_LENGTH") == NULL ? reader.GetReal("station", "length", 10) : stod(getenv("VANETZA_LENGTH"));
    config_s->width = getenv("VANETZA_WIDTH") == NULL ? reader.GetReal("station", "width", 3) : stod(getenv("VANETZA_WIDTH"));
    config_s->interface = getenv("VANETZA_INTERFACE") == NULL ? reader.Get("general", "interface", "wlan0") : getenv("VANETZA_INTERFACE");
    config_s->local_mqtt_broker = getenv("VANETZA_LOCAL_MQTT_BROKER") == NULL ? reader.Get("general", "local_mqtt_broker", "127.0.0.1") : getenv("VANETZA_LOCAL_MQTT_BROKER");
    config_s->local_mqtt_port = getenv("VANETZA_LOCAL_MQTT_PORT") == NULL ? reader.GetInteger("general", "local_mqtt_port", 1883) : stoi(getenv("VANETZA_LOCAL_MQTT_PORT"));
    config_s->remote_mqtt_broker = getenv("VANETZA_REMOTE_MQTT_BROKER") == NULL ? reader.Get("general", "remote_mqtt_broker", "") : getenv("VANETZA_REMOTE_MQTT_BROKER");
    config_s->remote_mqtt_port = getenv("VANETZA_REMOTE_MQTT_PORT") == NULL ? reader.GetInteger("general", "remote_mqtt_port", 0) : stoi(getenv("VANETZA_REMOTE_MQTT_PORT"));
    config_s->remote_mqtt_username = getenv("VANETZA_REMOTE_MQTT_USERNAME") == NULL ? reader.Get("general", "remote_mqtt_username", "") : getenv("VANETZA_REMOTE_MQTT_USERNAME");
    config_s->remote_mqtt_password = getenv("VANETZA_REMOTE_MQTT_PASSWORD") == NULL ? reader.Get("general", "remote_mqtt_password", "") : getenv("VANETZA_REMOTE_MQTT_PASSWORD");
    config_s->gpsd_host = getenv("VANETZA_GPSD_HOST") == NULL ? reader.Get("general", "gpsd_host", "127.0.0.1") : getenv("VANETZA_GPSD_HOST");
    config_s->gpsd_port = getenv("VANETZA_GPSD_PORT") == NULL ? reader.Get("general", "gpsd_port", "2947") : getenv("VANETZA_GPSD_PORT");
    config_s->prometheus_port = getenv("VANETZA_PROMETHEUS_PORT") == NULL ? reader.GetInteger("general", "prometheus_port", 9100) : stoi(getenv("VANETZA_PROMETHEUS_PORT"));
    config_s->rssi_enabled = getenv("VANETZA_RSSI_ENABLED") == NULL ? reader.GetBoolean("general", "rssi_enabled", true) : (strcmp((getenv("VANETZA_RSSI_ENABLED")), "true") == 0);
    config_s->mcs_enabled = getenv("VANETZA_MCS_ENABLED") == NULL ? reader.GetBoolean("general", "mcs_enabled", false) : (strcmp((getenv("VANETZA_MCS_ENABLED")), "true") == 0);
    config_s->ignore_own_messages = getenv("VANETZA_IGNORE_OWN_MESSAGES") == NULL ? reader.GetBoolean("general", "ignore_own_messages", true) : (strcmp((getenv("VANETZA_IGNORE_OWN_MESSAGES")), "true") == 0);
    config_s->ignore_rsu_messages = getenv("VANETZA_IGNORE_RSU_MESSAGES") == NULL ? reader.GetBoolean("general", "ignore_rsu_messages", false) : (strcmp((getenv("VANETZA_IGNORE_RSU_MESSAGES")), "true") == 0);
    config_s->enable_json_prints = getenv("VANETZA_ENABLE_JSON_PRINTS") == NULL ? reader.GetBoolean("general", "enable_json_prints", true) : (strcmp((getenv("VANETZA_ENABLE_JSON_PRINTS")), "true") == 0);
    config_s->dds_participant_name = getenv("VANETZA_DDS_PARTICIPANT_NAME") == NULL ? reader.Get("general", "dds_participant_name", "Vanetza") : getenv("VANETZA_DDS_PARTICIPANT_NAME");
    config_s->dds_domain_id = getenv("VANETZA_DDS_DOMAIN_ID") == NULL ? reader.GetInteger("general", "dds_domain_id", 0) : stoi(getenv("VANETZA_DDS_DOMAIN_ID"));
    config_s->num_threads = getenv("VANETZA_NUM_THREADS") == NULL ? reader.GetInteger("general", "num_threads", -1) : stoi(getenv("VANETZA_NUM_THREADS"));
    config_s->publish_encoded_payloads = getenv("VANETZA_PUBLISH_ENCODED_PAYLOADS") == NULL ? reader.GetBoolean("general", "publish_encoded_payloads", false) : (strcmp((getenv("VANETZA_PUBLISH_ENCODED_PAYLOADS")), "true") == 0);
    config_s->debug_enabled = getenv("VANETZA_DEBUG_ENABLED") == NULL ? reader.GetBoolean("general", "debug_enabled", false) : (strcmp((getenv("VANETZA_DEBUG_ENABLED")), "true") == 0);
    config_s->zenoh_local_only = getenv("VANETZA_ZENOH_LOCAL_ONLY") == NULL ? reader.GetBoolean("general", "zenoh_local_only", true) : (strcmp((getenv("VANETZA_ZENOH_LOCAL_ONLY")), "true") == 0);
    config_s->zenoh_interfaces = getenv("VANETZA_ZENOH_INTERFACES") == NULL ? reader.Get("general", "zenoh_interfaces", "") : getenv("VANETZA_ZENOH_INTERFACES");
    config_s->cam = read_message_config(reader, "VANETZA_CAM", "cam");
    config_s->denm = read_message_config(reader, "VANETZA_DENM", "denm");
    config_s->cpm = read_message_config(reader, "VANETZA_CPM", "cpm");
    config_s->vam = read_message_config(reader, "VANETZA_VAM", "vam");
    config_s->spatem = read_message_config(reader, "VANETZA_SPATEM", "spatem");
    config_s->mapem = read_message_config(reader, "VANETZA_MAPEM", "mapem");
    config_s->ssem = read_message_config(reader, "VANETZA_SSEM", "ssem");
    config_s->srem = read_message_config(reader, "VANETZA_SREM", "srem");
    config_s->rtcmem = read_message_config(reader, "VANETZA_RTCMEM", "rtcmem");
    config_s->ivim = read_message_config(reader, "VANETZA_IVIM", "ivim");
    config_s->imzm = read_message_config(reader, "VANETZA_IMZM", "imzm");
    config_s->evcsnm = read_message_config(reader, "VANETZA_EVCSNM", "evcsnm");
    config_s->evrsrm = read_message_config(reader, "VANETZA_EVRCSM", "evrsrm");
    config_s->tistpgm = read_message_config(reader, "VANETZA_TISTPGM", "tistpgm");
    config_s->mcm = read_message_config(reader, "VANETZA_MCM", "mcm");
    // config_s->full_cam_topic_in = getenv("VANETZA_CAM_FULL_TOPIC_IN") == NULL ? reader.Get("cam", "full_topic_in", "") : getenv("VANETZA_CAM_FULL_TOPIC_IN");
    // config_s->full_cam_topic_out = getenv("VANETZA_CAM_FULL_TOPIC_OUT") == NULL ? reader.Get("cam", "full_topic_out", "") : getenv("VANETZA_CAM_FULL_TOPIC_OUT");
    // config_s->full_cam_topic_time = getenv("VANETZA_CAM_FULL_TOPIC_TIME") == NULL ? reader.Get("cam", "full_topic_time", "") : getenv("VANETZA_CAM_FULL_TOPIC_TIME");
    // config_s->full_cam_topic_test = getenv("VANETZA_CAM_FULL_TOPIC_TEST") == NULL ? reader.Get("cam", "full_topic_test", "") : getenv("VANETZA_CAM_FULL_TOPIC_TEST");
    // config_s->full_vam_topic_in = getenv("VANETZA_VAM_FULL_TOPIC_IN") == NULL ? reader.Get("vam", "full_topic_in", "") : getenv("VANETZA_VAM_FULL_TOPIC_IN");
    // config_s->full_vam_topic_out = getenv("VANETZA_VAM_FULL_TOPIC_OUT") == NULL ? reader.Get("vam", "full_topic_out", "") : getenv("VANETZA_VAM_FULL_TOPIC_OUT");
    // config_s->full_vam_topic_time = getenv("VANETZA_VAM_FULL_TOPIC_TIME") == NULL ? reader.Get("vam", "full_topic_time", "") : getenv("VANETZA_VAM_FULL_TOPIC_TIME");
    // config_s->full_vam_topic_test = getenv("VANETZA_VAM_FULL_TOPIC_TEST") == NULL ? reader.Get("vam", "full_topic_test", "") : getenv("VANETZA_VAM_FULL_TOPIC_TEST");
    config_s->own_cam_topic_out = getenv("VANETZA_CAM_OWN_TOPIC_OUT") == NULL ? reader.Get("cam", "own_topic_out", "") : getenv("VANETZA_CAM_OWN_TOPIC_OUT");
    // config_s->own_full_cam_topic_out = getenv("VANETZA_CAM_OWN_FULL_TOPIC_OUT") == NULL ? reader.Get("cam", "own_full_topic_out", "") : getenv("VANETZA_CAM_OWN_FULL_TOPIC_OUT");
}
