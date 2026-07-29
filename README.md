<div align="center">

# ⚡ Industrial IoT Failure Management System
### AI-Driven Predictive Maintenance with Edge Anomaly Detection

![Platform](https://img.shields.io/badge/Platform-ESP32-blue?style=for-the-badge&logo=espressif)
![Language](https://img.shields.io/badge/Firmware-C%2B%2B%20%2F%20Arduino-orange?style=for-the-badge&logo=arduino)
![Backend](https://img.shields.io/badge/Backend-Python%20Flask-green?style=for-the-badge&logo=flask)
![Cost](https://img.shields.io/badge/Total%20Cost-Under%20%2415-red?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-purple?style=for-the-badge)

*A complete hardware-software solution for real-time motor health monitoring, statistical anomaly detection, and automated emergency shutdown — all running on a $4.50 microcontroller.*

</div>

---

## 📖 Overview

Industrial machinery failures cause **42% of unplanned production downtime** globally, costing manufacturers an estimated **$50 billion annually**. Traditional reactive and preventive maintenance strategies are either too late or too wasteful.

This system implements **Predictive Maintenance** — monitoring actual machine health indicators in real time and triggering intervention only when statistically significant degradation is detected. The key innovation is that all anomaly detection runs **directly on the ESP32 microcontroller at the edge**, with no cloud dependency, no pre-trained models, and no network latency for safety-critical shutdown decisions.

### What it does
- 📡 **Monitors** vibration (MPU6050), temperature, and humidity (DHT22) at 1 Hz
- 🧠 **Learns** the machine's normal operating signature through a self-calibrating phase using Welford's Online Algorithm
- ⚠️ **Detects** statistical anomalies in real time using dynamic Z-score analysis
- ⛔ **Shuts down** the motor automatically within **< 10 ms** of a confirmed fault
- 📊 **Streams** all telemetry to a live web dashboard over Wi-Fi
- 🔄 **Recovers** remotely with a single button click from the dashboard

---

## 🏗️ System Architecture

The system follows a **three-tier edge-first architecture**:

```
┌───────────────────────────────────────────────────────┐
│                    SENSING LAYER                      │
│  MPU6050 (Vibration) ──┐                              │
│  DHT22 (Temp/Humidity)─┤──► ESP32 Edge Compute Node  │
│  Motor + Relay Circuit─┘                              │
└─────────────────────────────┬─────────────────────────┘
                              │ Wi-Fi (HTTP/JSON)
                              ▼
┌───────────────────────────────────────────────────────┐
│                EDGE INTELLIGENCE LAYER                │
│  Welford's Online Algorithm (Running Mean + Variance) │
│  Dynamic Z-Score Computation (per sensor)             │
│  Anomaly Streak Counter (debounce filter)             │
│  GPIO Motor Trip Logic (< 10 ms latency)              │
└─────────────────────────────┬─────────────────────────┘
                              │ REST API
                              ▼
┌───────────────────────────────────────────────────────┐
│               VISUALIZATION LAYER                     │
│  Python Flask Backend  ──► Live Sensor Charts         │
│  REST API Endpoints    ──► AI Health Score (0–100%)   │
│  Alert Log Storage     ──► Fault History & Logs       │
│  Remote Reset Control  ──► Motor Status Panel         │
└───────────────────────────────────────────────────────┘
```

---

## 📁 Repository Structure

```text
Motor_failure_management/
├── esp32_code/
│   └── esp32_code.ino         # ESP32 C++ firmware
│                              #  ├─ I2C sensor init & bus recovery
│                              #  ├─ Welford's online statistics (per sensor)
│                              #  ├─ Z-score anomaly detection
│                              #  ├─ Motor GPIO trip logic
│                              #  └─ Wi-Fi HTTP telemetry client
│
├── dashboard/
│   ├── server.py              # Flask REST API backend
│   │                          #  ├─ POST /api/data  (telemetry ingestion)
│   │                          #  ├─ POST /api/alert (fault logging)
│   │                          #  ├─ GET  /api/status (dashboard polling)
│   │                          #  └─ POST /api/reset  (motor recovery)
│   └── static/
│       └── index.html         # Frontend dashboard (charts, alerts, controls)
│
├── case_study_report.tex      # Full IEEE-style technical report (LaTeX)
├── requirements.txt           # Python dependencies
├── .gitignore
└── README.md
```

---

## 🧠 The AI/ML Engine: Welford's Online Algorithm

The anomaly detection runs **entirely on the ESP32** using Welford's numerically stable online algorithm. This computes a rolling mean and sample variance in a single pass without storing any historical data — critical for a microcontroller with 520 KB of SRAM.

**Per sensor update (every reading):**

$$\delta_1 = x_n - \bar{x}_{n-1}, \quad \bar{x}_n = \bar{x}_{n-1} + \frac{\delta_1}{n}, \quad M_{2,n} = M_{2,n-1} + \delta_1 \cdot (x_n - \bar{x}_n), \quad \sigma^2_n = \frac{M_{2,n}}{n-1}$$

**After calibration (≥ 10 samples), the Z-score is computed as:**

$$Z = \frac{|x - \bar{x}|}{\max(\sigma, \; \sigma_{\min})}$$

The `σ_min` **noise floor** (sensor-specific) prevents false triggers when variance is near zero during very stable operation.

### Detection Thresholds

| Sensor | Threshold | Noise Floor (σ_min) |
|--------|-----------|----------------------|
| Vibration (MPU6050) | Z > **3.0σ** | 0.02 g |
| Temperature (DHT22) | Z > **4.5σ** | 0.5 °C |
| Humidity (DHT22) | Z > **7.0σ** | 1.0 % RH |

An **anomaly streak counter** requires **2 consecutive anomalous readings** before triggering a fault, acting as a debounce filter against single-sample noise spikes.

### 3D Vibration Magnitude

Instead of monitoring individual X/Y/Z axes (which requires precise sensor orientation), the firmware computes the orientation-independent **Euclidean magnitude**:

$$\|\mathbf{a}\| = \sqrt{a_x^2 + a_y^2 + a_z^2}$$

At rest, this equals approximately **1.0 g** regardless of how the sensor is mounted. The system learns this exact baseline value during calibration.

### AI Health Score

The web dashboard computes a composite **0–100% health score**:

$$H = \max\left(0, \; 100 - \frac{Z_{vib} + Z_{temp} + Z_{humi}}{3} \times 30\right)$$

| Score Range | Status |
|-------------|--------|
| **> 70%** | 🟢 Healthy |
| **40% – 70%** | 🟡 Warning |
| **< 40%** | 🔴 Critical |

---

## 🛒 Bill of Materials

| Component | Key Specs | Cost (USD) |
|-----------|-----------|------------|
| **ESP32 DevKit V1** | Dual-core 240 MHz, Wi-Fi 802.11 b/g/n, 520 KB SRAM | $4.50 |
| **MPU6050 Module** | 3-axis accel + 3-axis gyro, I2C, ±4g, 16-bit ADC | $1.50 |
| **DHT22 Module** | -40~80°C (±0.5°C), 0-100% RH (±2%), digital | $2.00 |
| **DC Motor (3-6V)** | Small brush motor for prototype demo | $0.50 |
| **NPN Transistor 2N2222** | V_CE=40V, I_C=800mA, h_FE=100–300, TO-92 | $0.10 |
| **2.5 kΩ Resistor** | Base current limiter (~1.3 mA) | $0.02 |
| **Breadboard + Jumpers** | 830-point board, M-M & M-F wires | $2.50 |
| **USB Cable (Micro-B)** | Power and programming | $1.00 |
| | **Total** | **$12.12** |

---

## 🔌 Hardware Wiring

| Sensor / Module | ESP32 GPIO | Interface | Notes |
|-----------------|-----------|-----------|-------|
| MPU6050 SDA | **GPIO 21** | I2C Data | 3.3V logic, 100 kHz Standard Mode |
| MPU6050 SCL | **GPIO 22** | I2C Clock | 150 ms bus timeout (EMI protection) |
| DHT22 Data | **GPIO 4** | Digital | Module includes built-in 10 kΩ pull-up |
| Motor Relay | **GPIO 18** | GPIO → 2N2222 Base | HIGH = Motor ON, LOW = Motor TRIP |

**Motor Circuit:** `GPIO 18 → 2.5 kΩ → 2N2222 Base | Collector → Motor(–) | Motor(+) → +5V USB`

---

## 🚀 Quick Start

### Prerequisites
- Arduino IDE with **ESP32 board support** installed
- Python 3.8+
- Both the ESP32 and the dashboard computer on the **same Wi-Fi network**

### Step 1: Flash the ESP32 Firmware

1. Open `esp32_code/esp32_code.ino` in Arduino IDE.
2. Install the following libraries via Library Manager (`Sketch → Include Library → Manage Libraries`):
   - `DHT sensor library` (by Adafruit)
   - `Adafruit Unified Sensor`
3. Edit the configuration block at the top of the file:
   ```cpp
   const char *WIFI_SSID     = "YOUR_WIFI_SSID";
   const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   const char *SERVER_IP     = "YOUR_LAPTOP_LOCAL_IP"; // e.g. "192.168.1.100"
   const int   SERVER_PORT   = 5000;
   ```
4. Select your ESP32 board and port, then click **Upload**.
5. Open **Serial Monitor** at `115200 baud` to observe the boot diagnostics and calibration progress.

> 💡 **To find your laptop's local IP:** Run `ipconfig` on Windows or `ifconfig` on Linux/macOS and look for the IPv4 address on your Wi-Fi adapter.

### Step 2: Start the Dashboard Server

```bash
# Clone the repository
git clone https://github.com/Ashrit8982/Motor_failure_management.git
cd Motor_failure_management

# Install Python dependencies
pip install -r requirements.txt

# Start the server
python dashboard/server.py
```

### Step 3: Open the Dashboard

Navigate to **[http://localhost:5000](http://localhost:5000)** in your browser.

The dashboard will show a calibration progress bar for the first ~10 seconds while the ESP32 learns the baseline. After that, live telemetry and anomaly detection activate automatically.

---

## 🌐 REST API Reference

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/api/data` | Receives live telemetry JSON from ESP32. Returns `{"reset": true}` if a motor reset has been queued. |
| `POST` | `/api/alert` | Logs an emergency fault event from the ESP32 (triggered on every fault trip). |
| `GET` | `/api/status` | Returns current sensor readings, 30-point history, and full alert log for the dashboard. |
| `GET` | `/api/logs` | Returns the last 20 alert log entries. |
| `POST` | `/api/reset` | Queues a motor reset command. ESP32 picks this up on its next `/api/data` response. |

**Telemetry JSON payload format (ESP32 → Server):**
```json
{
  "temperature": 28.5,
  "humidity": 55.2,
  "vibration": 1.0213,
  "vib_zscore": 0.32,
  "temp_zscore": 0.15,
  "humi_zscore": 0.09,
  "motor_running": true,
  "system_fault": false,
  "fault_reason": "",
  "calibrated": true,
  "calibration_progress": 10,
  "calibration_total": 10,
  "vib_baseline_mean": 1.0189,
  "vib_baseline_std": 0.0234,
  "uptime": 142
}
```

---

## 📊 Performance Metrics

| Metric | Value |
|--------|-------|
| Sensor sampling rate | 1 Hz |
| Dashboard update rate | 2 seconds |
| Calibration duration | ~10 seconds |
| Fault detection latency | ≤ 2 seconds |
| **Motor shutdown latency** | **< 10 ms** (direct GPIO) |
| HTTP report overhead | < 50 ms |
| ESP32 SRAM usage | ~45% |
| Wi-Fi indoor range | ~15 m |
| Power consumption | ~0.5 W idle |
| False positive rate (stable) | **0%** |
| **Total hardware cost** | **$12.12 USD** |

---

## ⚙️ Self-Healing I2C Bus Recovery

Motor EMI can freeze the I2C bus, causing the ESP32 to hang. The firmware implements automatic bus recovery: before every MPU6050 read, it probes the bus with a zero-length transmission. If the bus is stuck (NACK), it reinitializes the I2C peripheral and re-wakes the MPU6050 — all without requiring a system reboot.

---

## 🛣️ Future Work

- **FFT-Based Frequency Analysis** — Detect specific fault signatures (bearing defects at characteristic frequencies, rotor imbalance harmonics)
- **Multi-Node Mesh Network** — Monitor multiple machines from a single dashboard via ESP-NOW or MQTT
- **TinyML / TFLite** — Port a TensorFlow Lite model for multi-class fault classification (bearing wear vs. misalignment vs. imbalance)
- **Cloud Integration** — Add MQTT publishing to AWS IoT Core or Google Cloud IoT for long-term archival and fleet analytics
- **Custom PCB** — Shielded traces, proper motor snubbers, and grounding for EMI hardening
- **Energy Harvesting** — Power the sensor node from the motor's own vibration energy


---

## 📜 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

<div align="center">
Made with ❤️ at NIT Rourkela · Department of Electronics and Instrumentation Engineering
</div>
