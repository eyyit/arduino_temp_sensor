#include <ESP8266WiFi.h>

WiFiClient carbon_client;

// Static buffer for building metric lines - avoids heap fragmentation
// Format: temp_sensor.{sensor_name}.{metric} {value} -1\n
// Max lengths: "temp_sensor." (12) + sensor_name (32) + "." (1) + metric (40) + " " (1) + value (20) + " -1\n" (4) + null (1) = ~111
#define CARBON_BUFFER_SIZE 128
static char carbon_buffer[CARBON_BUFFER_SIZE];

// Max connection attempts before giving up (allows other operations like OTA to proceed)
#define MAX_CARBON_CONNECT_ATTEMPTS 3

// Returns true if connected, false if gave up after max attempts
bool connectCarbonClient() {
  // Don't attempt if carbon_host is not configured
  if (carbon_host.length() == 0) {
    Serial.println(F("Carbon host not configured, skipping connection"));
    return false;
  }
  
  for (int attempt = 1; attempt <= MAX_CARBON_CONNECT_ATTEMPTS; attempt++) {
    Serial.print(F("Connecting to carbon host "));
    Serial.print(carbon_host);
    Serial.print(F(":"));
    Serial.print(CARBON_PORT);
    Serial.print(F(" (attempt "));
    Serial.print(attempt);
    Serial.print(F("/"));
    Serial.print(MAX_CARBON_CONNECT_ATTEMPTS);
    Serial.println(F(")"));
    
    carbon_client.connect(carbon_host.c_str(), CARBON_PORT);
    client_connect_attempts += 1;
    
    if (carbon_client.connected()) {
      Serial.println(F("Carbon client connected."));
      return true;
    }
    
    Serial.println(F("Carbon client connection failed!"));
    if (attempt < MAX_CARBON_CONNECT_ATTEMPTS) {
      delay(POST_INTERVAL);
    }
  }
  
  Serial.println(F("Giving up on carbon connection, will retry next cycle"));
  return false;
}

// Build complete metric line in a single buffer and send atomically
// This prevents fragmented/corrupted metric paths
// Format: metric value timestamp\n (timestamp -1 means "use current time")
void write_carbon(const char* metric, float value) {
  // Build the complete line in one buffer using snprintf (safe, no heap allocation)
  int len = snprintf(carbon_buffer, CARBON_BUFFER_SIZE,
                     "temp_sensor.%s.%s %.2f -1\n",
                     sensor_name.c_str(), metric, value);
  
  if (len > 0 && len < CARBON_BUFFER_SIZE) {
    Serial.print(F("Sending "));
    Serial.print(carbon_buffer);
    // Send entire metric line in one write - prevents fragmentation
    carbon_client.write(carbon_buffer, len);
  } else {
    Serial.print(F("ERROR: metric buffer overflow for "));
    Serial.println(metric);
  }
}

// Overload for unsigned 32-bit values (heap stats, millis, etc.)
void write_carbon(const char* metric, uint32_t value) {
  int len = snprintf(carbon_buffer, CARBON_BUFFER_SIZE,
                     "temp_sensor.%s.%s %lu -1\n",
                     sensor_name.c_str(), metric, value);
  
  if (len > 0 && len < CARBON_BUFFER_SIZE) {
    Serial.print(F("Sending "));
    Serial.print(carbon_buffer);
    carbon_client.write(carbon_buffer, len);
  } else {
    Serial.print(F("ERROR: metric buffer overflow for "));
    Serial.println(metric);
  }
}

// Overload for signed 32-bit values (RSSI, etc.)
void write_carbon(const char* metric, int32_t value) {
  int len = snprintf(carbon_buffer, CARBON_BUFFER_SIZE,
                     "temp_sensor.%s.%s %ld -1\n",
                     sensor_name.c_str(), metric, value);
  
  if (len > 0 && len < CARBON_BUFFER_SIZE) {
    Serial.print(F("Sending "));
    Serial.print(carbon_buffer);
    carbon_client.write(carbon_buffer, len);
  } else {
    Serial.print(F("ERROR: metric buffer overflow for "));
    Serial.println(metric);
  }
}

// Overload for uint8_t values (fragmentation percentage)
void write_carbon(const char* metric, uint8_t value) {
  write_carbon(metric, (uint32_t)value);
}

void handleCarbon() {
  if (!getWiFiStatus()) { return; }
  if (!carbon_client.connected()) {
    if (!connectCarbonClient()) {
      return;  // Connection failed, skip this cycle
    }
  }

  updateEnvironment();

  // Environment sensor readings (floats)
  write_carbon("altitude", env.altitude);
  write_carbon("celcius", env.celcius);
  write_carbon("dewpointC", env.dewpointC);
  write_carbon("dewpointF", env.dewpointF);
  write_carbon("fahrenheit", env.fahrenheit);
  write_carbon("humidity", env.humidity);
  write_carbon("pressure", env.pressure);
  
  // System health metrics (unsigned integers)
  write_carbon("free_heap", ESP.getFreeHeap());
  write_carbon("heap_fragmentation_percent", ESP.getHeapFragmentation());
  write_carbon("max_free_block_size", ESP.getMaxFreeBlockSize());
  write_carbon("wifi_connect_attempts", wifi_connect_attempts);
  write_carbon("client_connect_attempts", client_connect_attempts);
  write_carbon("uptimeMS", (uint32_t)millis());
  
  // RSSI is signed (can be negative)
  write_carbon("RSSI", (int32_t)WiFi.RSSI());
}
