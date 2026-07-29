#include <DHT.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>

// ===================== USER CONFIGURATION =====================
const char *WIFI_SSID = "*"; 
const char *WIFI_PASSWORD = "12345678";        
const char *SERVER_IP = "10.98.204.230";        
const int SERVER_PORT = 5000;
// ==============================================================

// --- Pin Definitions ---
#define MPU6050_ADDR 0x68
#define DHT_PIN 4 
#define DHT_TYPE DHT22
#define MOTOR_PIN 18 



// --- AI/ML Anomaly Detection Config ---
#define CALIBRATION_SAMPLES 10 
#define Z_SCORE_THRESHOLD_VIB 3.0 
#define Z_SCORE_THRESHOLD_TEM 4.5
#define Z_SCORE_THRESHOLD_HUM 7.0
#define ANOMALY_STREAK 2 

#define SENSOR_READ_INTERVAL 1000                       
#define REPORT_INTERVAL 1000 

// --- ML Model State (Online Statistics) ---
struct SensorStats {
  double mean;
  double variance;
  double m2; 
  int count;
  bool calibrated;
};

SensorStats vibStats = {0, 0, 0, 0, false};
SensorStats tempStats = {0, 0, 0, 0, false};
SensorStats humiStats = {0, 0, 0, 0, false};

// --- DHT22 Sensor Object ---
DHT dht(DHT_PIN, DHT_TYPE);

// --- System State ---
bool motorRunning = true;
bool systemFault = false;
String faultReason = "";
int anomalyStreak = 0;
unsigned long lastSensorRead = 0;
unsigned long lastReport = 0;
float currentTemp = 0.0;
float currentHumidity = 0.0;
float currentVibration = 0.0;
float vibZScore = 0.0;
float tempZScore = 0.0;
float humiZScore = 0.0;


void setup() {
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println(" Industrial IoT Failure Management System");
  Serial.println("========================================\n");

 
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(
      MOTOR_PIN,
      HIGH); 

 
  delay(1000); 
  Wire.begin(21, 22);
  Wire.setClock(100000); 
                         
  Wire.setTimeOut(
      150); 

  Serial.println("\n[DIAGNOSTICS] Running full I2C bus scan...");
  int nDevices = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf(
          "[DIAGNOSTICS] SUCCESS: Found I2C device at address 0x%02X\n",
          address);
      nDevices++;
      if (address == 0x69) {
        Serial.println(
            "[DIAGNOSTICS] Note: Your MPU6050 is at 0x69! Check your AD0 pin.");
      }
    }
  }
  if (nDevices == 0) {
    Serial.println("[DIAGNOSTICS] FATAL: 0 I2C devices found! Wires are "
                   "broken, SWAPPED, or the sensor is dead.");
  } else {
    initMPU6050();
  }

  
  dht.begin();
  Serial.println("[DIAGNOSTICS] Testing DHT22...");
  delay(1000); 
  float t_test = dht.readTemperature();
  if (isnan(t_test)) {
    Serial.println("[DIAGNOSTICS] ERROR! DHT22 read failed. Check data wiring "
                   "to GPIO 4 and the 10k resistor.");
  } else {
    Serial.printf("[DIAGNOSTICS] SUCCESS! DHT22 responded with %.1f C.\n",
                  t_test);
  }


  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[OK] WiFi Connected!");
    Serial.print("[OK] ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WARN] WiFi connection failed. Running in offline mode.");
  }

  Serial.println("\n--- Calibration Phase ---");
  Serial.println("Collecting baseline 'normal' data...");
  Serial.println("Keep the motor running normally during calibration.\n");
}


void loop() {
  unsigned long now = millis();

  // --- Read Sensors at Fixed Interval ---
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    readSensors();
    runAnomalyDetection();
  }

  yield(); 

  if (now - lastReport >= REPORT_INTERVAL) {
    lastReport = now;
    sendDataToDashboard();
    printSerialStatus();
  }

  yield(); // Feed the watchdog at end of loop
}


void initMPU6050() {
  // Wake up MPU6050
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B); 
  Wire.write(0x00); 
  Wire.endTransmission(true);
  delay(100);

 
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x1C); 
  Wire.write(0x08); 
  Wire.endTransmission(true);

  // Set low-pass filter to 44Hz (smooths vibration data)
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x1A); 
  Wire.write(0x03); 
  Wire.endTransmission(true);

  Serial.println("[OK] MPU6050 Initialized (range: +/-4g, LPF: 44Hz)");
}


void readSensors() {
  // --- Read MPU6050 Accelerometer (X, Y, Z) ---
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B); 
  byte error = Wire.endTransmission(false);
  if (error != 0) {
    Serial.println("[WARNING] I2C endTransmission failed. Skipping read.");
    return;
  }

  uint8_t bytesReceived = Wire.requestFrom(MPU6050_ADDR, 6, true);
  if (bytesReceived != 6) {
    Serial.println("[WARNING] I2C requestFrom failed. Re-initializing I2C...");
    Wire.begin(21, 22); // Try to reset bus
    Wire.setClock(100000);
    Wire.setTimeOut(150);
    return;
  }

  int16_t ax_raw = (Wire.read() << 8) | Wire.read();
  int16_t ay_raw = (Wire.read() << 8) | Wire.read();
  int16_t az_raw = (Wire.read() << 8) | Wire.read();

  // Convert to g-force (sensitivity for +/-4g = 8192 LSB/g)
  float ax = ax_raw / 8192.0;
  float ay = ay_raw / 8192.0;
  float az = az_raw / 8192.0;

  currentVibration = sqrt(ax * ax + ay * ay + az * az);

  // --- Read DHT22 Temperature & Humidity ---
  float t = dht.readTemperature(); 
  float h = dht.readHumidity();   

  
  if (!isnan(t)) {
    currentTemp = t;
  }
  if (!isnan(h)) {
    currentHumidity = h;
  }
}







void updateStats(SensorStats &stats, double newValue) {
  stats.count++;
  double delta = newValue - stats.mean;
  stats.mean += delta / stats.count;
  double delta2 = newValue - stats.mean;
  stats.m2 += delta * delta2;

  if (stats.count >= 2) {
    stats.variance = stats.m2 / (stats.count - 1);
  }

  if (stats.count >= CALIBRATION_SAMPLES && !stats.calibrated) {
    stats.calibrated = true;
  }
}

double getZScore(SensorStats &stats, double value, double minStdDev) {
  if (!stats.calibrated)
    return 0.0;

  double stddev = sqrt(stats.variance);



  if (stddev < minStdDev) {
    stddev = minStdDev;
  }

  if (stddev < 0.0001) {
    return 0.0;
  }

  return fabs((value - stats.mean) / stddev);
}

void runAnomalyDetection() {


  if (!vibStats.calibrated || !tempStats.calibrated || !humiStats.calibrated) {
    updateStats(vibStats, currentVibration);
    updateStats(tempStats, currentTemp);
    updateStats(humiStats, currentHumidity);

    if (vibStats.calibrated && tempStats.calibrated && humiStats.calibrated) {
      Serial.println("\n========================================");
      Serial.println(" CALIBRATION COMPLETE - AI Model Ready!");
      Serial.printf(" Vibration Baseline: mean=%.3f g, stddev=%.3f g\n",
                    vibStats.mean, sqrt(vibStats.variance));
      Serial.printf(" Temperature Baseline: mean=%.1f C, stddev=%.1f C\n",
                    tempStats.mean, sqrt(tempStats.variance));
      Serial.printf(" Humidity Baseline: mean=%.1f %%, stddev=%.1f %%\n",
                    humiStats.mean, sqrt(humiStats.variance));
      Serial.println("========================================\n");
    }
    return;
  }

  
  
  vibZScore = getZScore(vibStats, currentVibration,
                        0.02); // 
  tempZScore =
      getZScore(tempStats, currentTemp, 0.5); // 
  humiZScore = getZScore(humiStats, currentHumidity,
                         1.0); 

  bool isAnomaly = (vibZScore > Z_SCORE_THRESHOLD_VIB) ||
                   (tempZScore > Z_SCORE_THRESHOLD_TEM) ||
                   (humiZScore > Z_SCORE_THRESHOLD_HUM);

  if (isAnomaly) {
    anomalyStreak++;
    Serial.printf("[ML WARNING] Anomaly detected! Vib Z=%.2f, Temp Z=%.2f, "
                  "Humi Z=%.2f (streak: %d/%d)\n",
                  vibZScore, tempZScore, humiZScore, anomalyStreak,
                  ANOMALY_STREAK);

    if (anomalyStreak >= ANOMALY_STREAK && !systemFault) {
      triggerFault("ML_ANOMALY_DETECTED");
    }
  } else {
    anomalyStreak = 0;
  }
}






void triggerFault(String reason) {
  systemFault = true;
  motorRunning = false;
  faultReason = reason;

  
  digitalWrite(MOTOR_PIN, LOW);

  Serial.println("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  Serial.println("!! CRITICAL FAULT DETECTED           !!");
  Serial.printf("!! Reason: %s\n", reason.c_str());
  Serial.printf("!! Temp: %.1f C  |  Humidity: %.1f %%  |  Vibration: %.3f g\n",
                currentTemp, currentHumidity, currentVibration);
  Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

 
  sendAlert(reason);
}


void sendDataToDashboard() {
  if (WiFi.status() != WL_CONNECTED)
    return;

  HTTPClient http;
  String url =
      "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/api/data";
  http.begin(url);
  http.setTimeout(1000); 
  http.addHeader("Content-Type", "application/json");


  String calibrationStatus =
      (vibStats.calibrated && tempStats.calibrated) ? "true" : "false";
  String json = "{";
  json += "\"temperature\":" + String(currentTemp, 1) + ",";
  json += "\"humidity\":" + String(currentHumidity, 1) + ",";
  json += "\"vibration\":" + String(currentVibration, 4) + ",";
  json += "\"vib_zscore\":" + String(vibZScore, 2) + ",";
  json += "\"temp_zscore\":" + String(tempZScore, 2) + ",";
  json += "\"humi_zscore\":" + String(humiZScore, 2) + ",";
  json += "\"motor_running\":" + String(motorRunning ? "true" : "false") + ",";
  json += "\"system_fault\":" + String(systemFault ? "true" : "false") + ",";
  json += "\"fault_reason\":\"" + faultReason + "\",";
  json += "\"calibrated\":" + calibrationStatus + ",";
  json += "\"calibration_progress\":" +
          String(min(vibStats.count, CALIBRATION_SAMPLES)) + ",";
  json += "\"calibration_total\":" + String(CALIBRATION_SAMPLES) + ",";
  json += "\"vib_baseline_mean\":" + String(vibStats.mean, 4) + ",";
  json += "\"vib_baseline_std\":" + String(sqrt(vibStats.variance), 4) + ",";
  json += "\"temp_baseline_mean\":" + String(tempStats.mean, 1) + ",";
  json += "\"temp_baseline_std\":" + String(sqrt(tempStats.variance), 1) + ",";
  json += "\"humi_baseline_mean\":" + String(humiStats.mean, 1) + ",";
  json += "\"humi_baseline_std\":" + String(sqrt(humiStats.variance), 1) + ",";
  json += "\"uptime\":" + String(millis() / 1000);
  json += "}";

  yield(); 
  int httpCode = http.POST(json);
  yield(); 
  if (httpCode > 0) {
    
    String response = http.getString();
    if (response.indexOf("\"reset\":true") >= 0 ||
        response.indexOf("\"reset\": true") >= 0) {
      Serial.println("\n[RESET] Dashboard sent reset command!");
      resetFault();
    }
  } else {
    Serial.printf("[HTTP] Send failed: %s\n",
                  http.errorToString(httpCode).c_str());
  }
  http.end();
}

void resetFault() {
  systemFault = false;
  motorRunning = true;
  faultReason = "";
  anomalyStreak = 0;
  vibZScore = 0;
  tempZScore = 0;
  humiZScore = 0;

  
  digitalWrite(MOTOR_PIN, HIGH);

  Serial.println("========================================");
  Serial.println(" FAULT CLEARED - System Resumed!");
  Serial.println("========================================\n");
}

void sendAlert(String reason) {
  if (WiFi.status() != WL_CONNECTED)
    return;

  HTTPClient http;
  String url =
      "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/api/alert";
  http.begin(url);
  http.setTimeout(1000); 
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"type\":\"CRITICAL_FAULT\",";
  json += "\"reason\":\"" + reason + "\",";
  json += "\"temperature\":" + String(currentTemp, 1) + ",";
  json += "\"humidity\":" + String(currentHumidity, 1) + ",";
  json += "\"vibration\":" + String(currentVibration, 4) + ",";
  json += "\"vib_zscore\":" + String(vibZScore, 2) + ",";
  json += "\"temp_zscore\":" + String(tempZScore, 2) + ",";
  json += "\"humi_zscore\":" + String(humiZScore, 2) + ",";
  json += "\"timestamp\":" + String(millis());
  json += "}";

  int httpCode = http.POST(json);
  if (httpCode > 0) {
    Serial.println("[ALERT] Emergency alert sent to dashboard!");
  } else {
    Serial.printf("[ALERT] Failed to send alert: %s\n",
                  http.errorToString(httpCode).c_str());
  }
  http.end();
}






void printSerialStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] WiFi connection lost! Motor noise likely jammed "
                   "the radio or dropped the voltage.");
  }

  if (!vibStats.calibrated) {
    Serial.printf(
        "[CALIBRATING %d/%d] Temp: %.1f C | Humi: %.1f %% | Vib: %.3f g\n",
        vibStats.count, CALIBRATION_SAMPLES, currentTemp, currentHumidity,
        currentVibration);
  } else {
    Serial.printf("[LIVE] Temp: %.1f C (Z=%.1f) | Humi: %.1f %% (Z=%.1f) | "
                  "Vib: %.3f g (Z=%.1f) | Motor: %s | Fault: %s\n",
                  currentTemp, tempZScore, currentHumidity, humiZScore,
                  currentVibration, vibZScore, motorRunning ? "ON" : "OFF",
                  systemFault ? faultReason.c_str() : "NONE");
  }
}
