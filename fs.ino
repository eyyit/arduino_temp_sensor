#include <FS.h>

bool spiffs_initialized = false;

String readFile(const String filename) {
  if (!spiffs_initialized) {
    Serial.println(F("ERROR: SPIFFS not initialized, cannot read file"));
    return "";
  }
  
  if (!SPIFFS.exists(filename)) {
    Serial.print(F("File not found: "));
    Serial.println(filename);
    return "";
  }
  
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.print(F("ERROR: Failed to open file for reading: "));
    Serial.println(filename);
    return "";
  }
  
  String content = file.readString();
  file.close();
  Serial.print(F("Read "));
  Serial.print(filename);
  Serial.print(F(": \""));
  Serial.print(content);
  Serial.println(F("\""));
  return content;
}

bool writeFile(const String filename, String content) {
  if (!spiffs_initialized) {
    Serial.println(F("ERROR: SPIFFS not initialized, cannot write file"));
    return false;
  }
  
  File file = SPIFFS.open(filename, "w");
  if (!file) {
    Serial.print(F("ERROR: Failed to open file for writing: "));
    Serial.println(filename);
    return false;
  }
  
  size_t written = file.print(content);
  file.close();
  
  if (written != content.length()) {
    Serial.print(F("ERROR: Write incomplete for "));
    Serial.print(filename);
    Serial.print(F(" (wrote "));
    Serial.print(written);
    Serial.print(F(" of "));
    Serial.print(content.length());
    Serial.println(F(" bytes)"));
    return false;
  }
  
  Serial.print(F("Wrote "));
  Serial.print(filename);
  Serial.print(F(": \""));
  Serial.print(content);
  Serial.println(F("\""));
  return true;
}

void setupFS() {
  Serial.print(F("Initializing SPIFFS... "));
  spiffs_initialized = SPIFFS.begin();
  
  if (spiffs_initialized) {
    Serial.println(F("OK"));
    
    // Print filesystem info
    FSInfo fs_info;
    if (SPIFFS.info(fs_info)) {
      Serial.print(F("  Total: "));
      Serial.print(fs_info.totalBytes / 1024);
      Serial.println(F(" KB"));
      Serial.print(F("  Used: "));
      Serial.print(fs_info.usedBytes / 1024);
      Serial.println(F(" KB"));
    }
  } else {
    Serial.println(F("FAILED!"));
    Serial.println(F("WARNING: SPIFFS filesystem failed to initialize."));
    Serial.println(F("Configuration will not persist across reboots."));
    Serial.println(F("Try uploading filesystem image or formatting SPIFFS."));
  }
}
