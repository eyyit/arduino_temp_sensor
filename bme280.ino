#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1020.0)
#define BME_PIN 0x76

// Constants for Magnus formula
#define MAGNUS_A (17.27)  // constant (dimensionless)
#define MAGNUS_B (237.7)  // constant for temperature (°C)

Adafruit_BME280 bme;
bool bme_initialized = false;

float calculateDewPoint() {
  // Guard against log(0) or log(negative) which returns -inf/NaN
  // Humidity should be 0-100%, but sensor errors could return 0 or invalid values
  float humidity_safe = env.humidity;
  if (humidity_safe < 0.1) {
    humidity_safe = 0.1;  // Minimum to avoid log(0)
  } else if (humidity_safe > 100.0) {
    humidity_safe = 100.0;
  }
  
  // Calculate dew point in Celsius using Magnus-Tetens approximation
  float alpha = (
    (MAGNUS_A * env.celcius) / (MAGNUS_B + env.celcius)
  ) + log(humidity_safe / 100.0);
  float dewPointC = (MAGNUS_B * alpha) / (MAGNUS_A - alpha);

  return dewPointC;
}

float convertTempCtoF(float celcius) {
  return (celcius * 1.8F) + 32.0F;
}

void setupBME() {
  Serial.print(F("Initializing BME280 at 0x"));
  Serial.print(BME_PIN, HEX);
  Serial.print(F("... "));
  
  bme_initialized = bme.begin(BME_PIN);
  
  if (bme_initialized) {
    Serial.println(F("OK"));
  } else {
    Serial.println(F("FAILED!"));
    Serial.println(F("WARNING: BME280 sensor not found. Check wiring and I2C address."));
    Serial.println(F("Sensor readings will be invalid until sensor is connected."));
  }
}

void updateEnvironment() {
  if (!bme_initialized) {
    // Try to re-initialize on each read in case sensor was reconnected
    bme_initialized = bme.begin(BME_PIN);
    if (bme_initialized) {
      Serial.println(F("BME280 sensor recovered!"));
    } else {
      // Set values to NaN to indicate invalid readings
      env.altitude = NAN;
      env.celcius = NAN;
      env.fahrenheit = NAN;
      env.humidity = NAN;
      env.pressure = NAN;
      env.dewpointC = NAN;
      env.dewpointF = NAN;
      return;
    }
  }
  
  env.altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
  env.celcius = bme.readTemperature();
  env.fahrenheit = convertTempCtoF(env.celcius);
  env.humidity = bme.readHumidity();
  env.pressure = bme.readPressure() / 100.0F;
  env.dewpointC = calculateDewPoint();
  env.dewpointF = convertTempCtoF(env.dewpointC);
}
