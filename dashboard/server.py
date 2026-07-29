from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import time
from datetime import datetime

app = Flask(__name__, static_folder="static")
CORS(app)

latest_data = {}
data_history = []
alert_logs = []
reset_flag = False

MAX_HISTORY = 100
MAX_LOGS = 50


@app.route("/")
def index():
    return send_from_directory("static", "index.html")


@app.route("/<path:path>")
def static_files(path):
    return send_from_directory("static", path)


@app.route("/api/data", methods=["POST"])
def receive_data():
    global latest_data, reset_flag
    d = request.json
    d["timestamp"] = datetime.now().strftime("%H:%M:%S")
    d["time"] = time.time()
    latest_data = d

    data_history.append(d)
    if len(data_history) > MAX_HISTORY:
        data_history.pop(0)

    if d.get("system_fault") and d.get("fault_reason"):
        log_entry = {
            "timestamp": d["timestamp"],
            "reason": d["fault_reason"],
            "temperature": d.get("temperature", 0),
            "humidity": d.get("humidity", 0),
            "vibration": d.get("vibration", 0),
            "vib_zscore": d.get("vib_zscore", 0),
            "temp_zscore": d.get("temp_zscore", 0),
            "humi_zscore": d.get("humi_zscore", 0),
        }
        if not alert_logs or alert_logs[-1]["timestamp"] != d["timestamp"]:
            alert_logs.append(log_entry)
            if len(alert_logs) > MAX_LOGS:
                alert_logs.pop(0)

    should_reset = reset_flag
    reset_flag = False
    return jsonify({"status": "ok", "reset": should_reset})


@app.route("/api/alert", methods=["POST"])
def receive_alert():
    d = request.json
    log_entry = {
        "timestamp": datetime.now().strftime("%H:%M:%S"),
        "reason": d.get("reason", "UNKNOWN"),
        "temperature": d.get("temperature", 0),
        "humidity": d.get("humidity", 0),
        "vibration": d.get("vibration", 0),
        "vib_zscore": d.get("vib_zscore", 0),
        "temp_zscore": d.get("temp_zscore", 0),
        "humi_zscore": d.get("humi_zscore", 0),
    }
    alert_logs.append(log_entry)
    if len(alert_logs) > MAX_LOGS:
        alert_logs.pop(0)
    return jsonify({"status": "alert_logged"})


@app.route("/api/latest")
def get_latest():
    return jsonify(
        {
            "latest": latest_data,
            "history": data_history[-30:],
        }
    )


@app.route("/api/status")
def get_status():
    return jsonify(
        {
            "latest": latest_data,
            "history": data_history[-30:],
            "alerts": alert_logs,
        }
    )


@app.route("/api/logs")
def get_logs():
    return jsonify({"logs": alert_logs[-20:]})


@app.route("/api/reset", methods=["POST"])
def reset_motor():
    global reset_flag
    reset_flag = True
    return jsonify({"status": "reset_queued"})


if __name__ == "__main__":
    print("\n" + "=" * 50)
    print("  Industrial IoT Dashboard Server")
    print("  Open: http://localhost:5000")
    print("=" * 50 + "\n")
    app.run(host="0.0.0.0", port=5000, debug=True)
