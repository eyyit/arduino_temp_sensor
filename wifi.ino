#define LED 2
#include <WiFiManager.h>

WiFiManager wifi_manager;

// Parameters are created dynamically in setupWiFi() AFTER config is loaded from SPIFFS
// This ensures the form fields show the current saved values, not empty strings
WiFiManagerParameter* param_version_info = nullptr;
WiFiManagerParameter* param_carbon_host = nullptr;
WiFiManagerParameter* param_ota_password = nullptr;
WiFiManagerParameter* param_sensor_name = nullptr;
bool wifi_params_initialized = false;

// ============================================================================
// STATIC BUFFERS - WiFiManager stores pointers, so all strings must persist!
// ============================================================================

// Hostname/AP name: "sensor_" (7) + sensor_name (max 32) + null (1) = 40 bytes
#define HOSTNAME_BUFFER_SIZE 40
static char sensor_hostname[HOSTNAME_BUFFER_SIZE];

// Page title buffer - must persist
#define TITLE_BUFFER_SIZE 40
static char page_title[TITLE_BUFFER_SIZE];

// Version info HTML - needs enough space for full table (~900 bytes to be safe)
#define VERSION_INFO_BUFFER_SIZE 1024
static char version_info_buffer[VERSION_INFO_BUFFER_SIZE];

// Custom menu HTML - shows on ALL pages (info, config, param, etc.)
#define MENU_HTML_SIZE 512
static char custom_menu_html[MENU_HTML_SIZE];

// Parameter default values - must persist for WiFiManagerParameter
#define PARAM_VALUE_SIZE 42
static char param_carbon_host_value[PARAM_VALUE_SIZE];
static char param_ota_password_value[PARAM_VALUE_SIZE];
static char param_sensor_name_value[PARAM_VALUE_SIZE];

// ESP core version (returned as temporary String, need to cache it)
static char esp_core_version[20];


void connectWiFi() {
  Serial.println("WiFi Connecting...");
  setupWiFi();
}

bool getWiFiStatus() {
  return (WiFi.status() == WL_CONNECTED);
}

void handleWiFi() {
  digitalWrite(LED, HIGH);
  if (getWiFiStatus()) { return; }
  Serial.println("WiFi Reconnecting...");
  wifi_connect_attempts += 1;
  setupWiFi();
}

void handleHTTPServer() {
  wifi_manager.process();
}

void setupWiFi() {
  // =========================================================================
  // STEP 1: Populate ALL static buffers FIRST, before any WiFiManager calls
  // =========================================================================
  
  // Build hostname/AP name into static buffer
  snprintf(sensor_hostname, HOSTNAME_BUFFER_SIZE, "sensor_%s", sensor_name.c_str());
  
  // Build page title into static buffer
  snprintf(page_title, TITLE_BUFFER_SIZE, "%s", sensor_name.c_str());
  
  // Cache ESP core version (returns temporary String)
  strncpy(esp_core_version, ESP.getCoreVersion().c_str(), sizeof(esp_core_version) - 1);
  esp_core_version[sizeof(esp_core_version) - 1] = '\0';
  
  // Copy parameter default values into static buffers
  strncpy(param_carbon_host_value, carbon_host.c_str(), PARAM_VALUE_SIZE - 1);
  param_carbon_host_value[PARAM_VALUE_SIZE - 1] = '\0';
  
  strncpy(param_ota_password_value, ota_password.c_str(), PARAM_VALUE_SIZE - 1);
  param_ota_password_value[PARAM_VALUE_SIZE - 1] = '\0';
  
  strncpy(param_sensor_name_value, sensor_name.c_str(), PARAM_VALUE_SIZE - 1);
  param_sensor_name_value[PARAM_VALUE_SIZE - 1] = '\0';
  
  // Build custom menu HTML with version table (appears on ALL pages including info)
  snprintf(custom_menu_html, MENU_HTML_SIZE,
      "<div style='background:#f0f0f0;padding:8px;margin:10px 0;border-radius:5px;'>"
      "<div style='text-align:center;font-size:1.2em;font-weight:bold;margin-bottom:5px;'>%s</div>"
      "<table style='width:100%%;font-size:0.9em;'>"
      "<tr><td>Version</td><td><b>%s</b></td></tr>"
      "<tr><td>ESP Core</td><td>%s</td></tr>"
      "<tr><td>SDK</td><td>%s</td></tr>"
      "</table></div>",
      param_sensor_name_value,
      VERSION,
      esp_core_version,
      ESP.getSdkVersion());
  
  // Build simpler version info for param page (custom menu already has details)
  snprintf(version_info_buffer, VERSION_INFO_BUFFER_SIZE,
      "<hr><b>Firmware: %s v%s</b><hr>",
      param_sensor_name_value,
      VERSION);
  
  // Debug output
  Serial.print(F("Hostname buffer: ")); Serial.println(sensor_hostname);
  Serial.print(F("Title buffer: ")); Serial.println(page_title);
  
  // =========================================================================
  // STEP 2: Configure WiFiManager with static buffers
  // =========================================================================
  
  WiFi.hostname(sensor_hostname);
  Serial.println("MAC address: " + WiFi.macAddress());
  Serial.println("Connecting to SSID: " + WiFi.SSID());

  // Set title BEFORE any portal operations (use static buffer!)
  wifi_manager.setTitle(page_title);
  
  // Add custom HTML to menu (shows version on all pages)
  wifi_manager.setCustomMenuHTML(custom_menu_html);
  
  wifi_manager.setConfigPortalTimeout(60);
  wifi_manager.setSaveParamsCallback(saveParams);

  // Create parameters only once
  if (!wifi_params_initialized) {
    // Create parameters with static buffer values
    param_version_info = new WiFiManagerParameter(version_info_buffer);
    param_carbon_host = new WiFiManagerParameter(
        "carbon_host", "Carbon Host", param_carbon_host_value, 41);
    param_ota_password = new WiFiManagerParameter(
        "ota_password", "OTA Password", param_ota_password_value, 41);
    param_sensor_name = new WiFiManagerParameter(
        "sensor_name", "Sensor Name", param_sensor_name_value, 41);
    
    wifi_manager.addParameter(param_version_info);
    wifi_manager.addParameter(param_carbon_host);
    wifi_manager.addParameter(param_ota_password);
    wifi_manager.addParameter(param_sensor_name);
    
    wifi_params_initialized = true;
    
    Serial.println(F("WiFi parameters initialized:"));
    Serial.print(F("  carbon_host: ")); Serial.println(param_carbon_host_value);
    Serial.print(F("  ota_password: ")); Serial.println(strlen(param_ota_password_value) > 0 ? "****" : "(empty)");
    Serial.print(F("  sensor_name: ")); Serial.println(param_sensor_name_value);
  }

  // =========================================================================
  // STEP 3: Connect to WiFi
  // =========================================================================
  
  analogWrite(LED, 10);
  // Open config portal if carbon_host is not configured
  if (carbon_host.length() == 0) {
    Serial.println(F("Carbon host not configured, starting config portal..."));
    wifi_manager.startConfigPortal(sensor_hostname, NULL);
  }
  if (!wifi_manager.autoConnect(sensor_hostname)) {
    Serial.println("Failed connecting to WiFi and timed out. Rebooting...");
    ESP.reset();
  }

  while (!getWiFiStatus()) {
    delay(30);
  }
  analogWrite(LED, 0);
  wifi_manager.stopConfigPortal();
  wifi_manager.setCaptivePortalEnable(false);
  wifi_manager.startWebPortal();
  WiFi.mode(WIFI_STA);
  Serial.print("Hostname: ");
  Serial.println(WiFi.hostname());
  Serial.print(F("Primary DNS Server: "));
  Serial.println(WiFi.dnsIP(0));
  Serial.print(F("Secondary DNS Server: "));
  Serial.println(WiFi.dnsIP(1));
  Serial.print(F("Gateway: "));
  Serial.println(WiFi.gatewayIP());
  Serial.print(F("Subnet Mask: "));
  Serial.println(WiFi.subnetMask());
}

void saveParams() {
  Serial.println(F("saveParams() called - checking for changes..."));
  
  if (!param_carbon_host || !param_ota_password || !param_sensor_name) {
    Serial.println(F("ERROR: Parameters not initialized!"));
    return;
  }
  
  String value;

  value = param_carbon_host->getValue();
  // Only save if value is non-empty AND different from current
  // This prevents accidentally clearing config when form fields are empty
  if (value.length() > 0 && !carbon_host.equals(value)) {
    Serial.print(F("Updating carbon_host: "));
    Serial.print(carbon_host);
    Serial.print(F(" -> "));
    Serial.println(value);
    carbon_host = value;
    writeFile("/carbon_host", carbon_host);
  } else if (value.length() == 0 && carbon_host.length() > 0) {
    Serial.println(F("Keeping existing carbon_host (form was empty)"));
  }

  value = param_ota_password->getValue();
  if (value.length() > 0 && !ota_password.equals(value)) {
    Serial.println(F("Updating ota_password"));
    ota_password = value;
    writeFile("/ota_password", ota_password);
  } else if (value.length() == 0 && ota_password.length() > 0) {
    Serial.println(F("Keeping existing ota_password (form was empty)"));
  }

  value = param_sensor_name->getValue();
  if (value.length() > 0 && !sensor_name.equals(value)) {
    Serial.print(F("Updating sensor_name: "));
    Serial.print(sensor_name);
    Serial.print(F(" -> "));
    Serial.println(value);
    sensor_name = value;
    writeFile("/sensor_name", sensor_name);
  } else if (value.length() == 0 && sensor_name.length() > 0) {
    Serial.println(F("Keeping existing sensor_name (form was empty)"));
  }
  
  Serial.println(F("saveParams() complete"));
}
