# Industrial IoT Failure Management System with Edge Anomaly Detection

An integrated hardware-software solution designed for predictive maintenance, real-time telemetry monitoring, and automated emergency safety cutoff in industrial motor operations.

The system combines an **ESP32 microcontroller** executing edge anomaly detection using statistical Z-Score analysis, a **Python Flask backend** for real-time telemetry ingestion, an interactive **Web Dashboard** UI, and comprehensive **LaTeX technical documentation**.

---

## Key Features

- **Edge Anomaly Detection (ESP32)**:
  - Uses **Welford's Algorithm** for online, single-pass running mean and sample variance calculation.
  - Computes dynamic **Z-scores** for 3-axis vibration (`MPU6050`) and temperature/humidity (`DHT22`).
  - Self-calibrating baseline phase upon system bootup.
- **Hardware Emergency Control**:
  - Automatic hardware trip/motor relay cutoff when vibration or temperature anomalies cross critical Z-score thresholds ($Z_{vib} > 3.0$).
  - Prevents physical damage during unexpected mechanical or thermal faults.
- **Real-Time Web Dashboard**:
  - Telemetry streaming over REST JSON APIs.
  - Interactive visualization of live sensor readings, Z-score trends, baseline statistics, and critical alert history.
  - Remote manual motor reset trigger from the dashboard interface.
- **Technical Case Study**:
  - Full LaTeX report (`case_study_report.tex`) documenting system architecture, hardware selection, mathematical formulations, and experimental evaluation.

---

## Repository Structure

```text
failure-management/
├── esp32_code/
│   └── esp32_code.ino         # ESP32 C++ firmware (Sensors, Z-score ML model, Wi-Fi REST client)
├── dashboard/
│   ├── server.py              # Flask server handling API endpoints and alert logging
│   └── static/
│       └── index.html         # Frontend web dashboard (Live charts, stats, fault indicators)
├── case_study_report.tex      # Comprehensive LaTeX project report
├── requirements.txt           # Python dependencies for the dashboard server
└── README.md                  # Project documentation
```

---

## Hardware Requirements

- **ESP32 Development Board**
- **MPU6050** 6-DOF Accelerometer / Gyroscope (I2C)
- **DHT22 / AM2302** Temperature & Humidity Sensor
- **Relay Module / Motor Circuit** connected to GPIO pin 18
- **Connecting Wires & Power Supply**

### Hardware Wiring

| Sensor / Module | ESP32 Pin | Interface / Notes |
| :--- | :--- | :--- |
| **MPU6050 SDA** | GPIO 21 | I2C Data |
| **MPU6050 SCL** | GPIO 22 | I2C Clock |
| **DHT22 Data** | GPIO 4 | Pull-up resistor recommended |
| **Motor Relay** | GPIO 18 | High = Active / Low = Tripped |

---

## Quick Start Guide

### 1. ESP32 Firmware Setup

1. Open `esp32_code/esp32_code.ino` in **Arduino IDE**.
2. Install required Arduino libraries:
   - `DHT sensor library` by Adafruit
   - `Adafruit Unified Sensor`
3. Configure Wi-Fi and Server IP settings at the top of the file:
   ```cpp
   const char *WIFI_SSID = "YOUR_WIFI_SSID";
   const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   const char *SERVER_IP = "YOUR_LOCAL_IP_ADDRESS";
   const int SERVER_PORT = 5000;
   ```
4. Flash the code to your ESP32.

### 2. Dashboard Server Setup

1. Install Python dependencies:
   ```bash
   pip install -r requirements.txt
   ```
2. Start the Flask server:
   ```bash
   python dashboard/server.py
   ```
3. Open your browser and navigate to:
   ```text
   http://localhost:5000
   ```

---

## REST API Endpoints

- `POST /api/data`: Ingests live telemetry payload from ESP32. Returns motor reset flag state.
- `POST /api/alert`: Logs emergency fault events triggered by edge anomaly detection.
- `GET /api/status`: Fetches live metrics, recent history, and alert logs for dashboard rendering.
- `POST /api/reset`: Queues a motor reset command to re-enable motor operation after a fault trip.

---

## License

This project is licensed under the MIT License.
