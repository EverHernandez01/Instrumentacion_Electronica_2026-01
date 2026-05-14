"""
IoT Systems Design - Lab Dashboard (Wi-Fi / HTTP Version)
Application and Service Domain (ASD) Server
"""

from flask import Flask, render_template_string, request, jsonify
import requests
import logging

app = Flask(__name__)
log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)

# --- Network Configuration ---
# replace this with te IPv4 address their ESP32 gets from the Wi-Fi router
ESP32_IP = "10.177.54.95"  

def get_sensor_data():
    """Sensing Capability: Polls the ESP32 via HTTP GET."""
    try:
        # Assuming the ESP32 serves JSON at this endpoint
        response = requests.get(f"http://{ESP32_IP}/api/sensor", timeout=2)
        if response.status_code == 200:
            return response.json()
        return {"error": f"HTTP {response.status_code}"}
    except requests.exceptions.RequestException:
        return {"error": "Node Unreachable"}

def control_light(state):
    """Actuating Capability: Sends command to ESP32 via HTTP POST."""
    try:
        payload = {"state": state}
        response = requests.post(f"http://{ESP32_IP}/api/control", json=payload, timeout=2)
        return response.status_code == 200
    except requests.exceptions.RequestException:
        return False

# --- Frontend User Domain (UD) ---
HTML_TEMPLATE = '''
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 IoT Dashboard</title>

    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

    <style>

        *{
            margin:0;
            padding:0;
            box-sizing:border-box;
        }

        body{
            font-family: 'Segoe UI', sans-serif;
            background: linear-gradient(135deg,#0f172a,#1e293b);
            color:white;
            min-height:100vh;
            padding:30px;
        }

        h1{
            text-align:center;
            margin-bottom:10px;
            font-size:2.5rem;
        }

        .subtitle{
            text-align:center;
            color:#94a3b8;
            margin-bottom:30px;
        }

        .dashboard{
            display:grid;
            grid-template-columns: repeat(auto-fit,minmax(320px,1fr));
            gap:20px;
        }

        .card{
            background: rgba(255,255,255,0.08);
            backdrop-filter: blur(10px);
            border-radius:20px;
            padding:25px;
            box-shadow: 0 8px 20px rgba(0,0,0,0.3);
            transition:0.3s;
        }

        .card:hover{
            transform:translateY(-5px);
        }

        h3{
            margin-bottom:20px;
            font-size:1.3rem;
        }

        .status{
            margin-top:15px;
            color:#cbd5e1;
        }

        .led-indicator{
            width:70px;
            height:70px;
            border-radius:50%;
            background:#222;
            margin:20px auto;
            box-shadow: 0 0 20px rgba(0,0,0,0.5);
            transition:0.3s;
        }

        .btn-group{
            display:flex;
            gap:15px;
            margin-top:20px;
        }

        .btn{
            flex:1;
            padding:15px;
            border:none;
            border-radius:12px;
            font-size:1rem;
            font-weight:bold;
            cursor:pointer;
            transition:0.3s;
            color:white;
        }

        .btn:hover{
            transform:scale(1.05);
        }

        .btn-on{
            background:#22c55e;
        }

        .btn-off{
            background:#ef4444;
        }

        .color-picker-container{
            margin-top:25px;
            text-align:center;
        }

        input[type="color"]{
            width:120px;
            height:60px;
            border:none;
            border-radius:12px;
            cursor:pointer;
            background:none;
        }

        .telemetry-value{
            font-size:2rem;
            text-align:center;
            margin-top:15px;
            color:#38bdf8;
        }

        canvas{
            background:white;
            border-radius:15px;
            padding:10px;
        }

    </style>
</head>

<body>

    <h1>ESP32 IoT Dashboard</h1>
    <p class="subtitle">Wi-Fi Remote Monitoring & LED RGB Control</p>

    <div class="dashboard">

        <!-- TELEMETRY -->
        <div class="card">

            <h3>Live Temperature Monitoring</h3>

            <canvas id="telemetryChart" height="120"></canvas>

            <div class="telemetry-value" id="temp-value">
                -- °C
            </div>

            <p class="status" id="conn-status">
                Waiting for sensor data...
            </p>

        </div>

        <!-- LED CONTROL -->
        <div class="card">

            <h3>RGB LED Control</h3>

            <div class="led-indicator" id="ledIndicator"></div>

            <div class="btn-group">

                <button class="btn btn-on"
                    onclick="controlLight(1)">
                    TURN ON
                </button>

                <button class="btn btn-off"
                    onclick="controlLight(0)">
                    TURN OFF
                </button>

            </div>

            <div class="color-picker-container">

                <p style="margin-bottom:10px;">
                    Select LED Color
                </p>

                <input type="color"
                    id="colorPicker"
                    value="#00ff00"
                    onchange="changeColor(this.value)">

            </div>

        </div>

    </div>

<script>

    // =========================
    // CHART CONFIGURATION
    // =========================

    const ctx = document.getElementById('telemetryChart').getContext('2d');

    const chart = new Chart(ctx, {

        type: 'line',

        data: {
            labels: [],
            datasets: [{
                label: 'Temperature °C',
                borderColor: '#38bdf8',
                backgroundColor: 'rgba(56,189,248,0.2)',
                data: [],
                tension: 0.3,
                fill:true
            }]
        },

        options: {
            responsive:true,
            animation:false,
            scales:{
                y:{
                    beginAtZero:false
                }
            }
        }

    });

    // =========================
    // UPDATE CHART
    // =========================

    function updateChart(temp){

        const time = new Date().toLocaleTimeString();

        chart.data.labels.push(time);
        chart.data.datasets[0].data.push(temp);

        if(chart.data.labels.length > 15){

            chart.data.labels.shift();
            chart.data.datasets[0].data.shift();

        }

        chart.update();

        document.getElementById("temp-value").innerText =
            temp + " °C";
    }

    // =========================
    // FETCH SENSOR DATA
    // =========================

    function fetchSensorData(){

        fetch('/api/sensor')

        .then(res => res.json())

        .then(data => {

            const status =
                document.getElementById('conn-status');

            if(data.error){

                status.innerText =
                    "Connection Error";

                status.style.color = "#ef4444";

            }

            else if(data.temperature){

                status.innerText =
                    "ESP32 Connected";

                status.style.color = "#22c55e";

                updateChart(data.temperature);

            }

        });

    }

    // =========================
    // LED ON/OFF
    // =========================

    function controlLight(state){

        fetch('/api/control', {

            method:'POST',

            headers:{
                'Content-Type':'application/json'
            },

            body: JSON.stringify({
                state: state
            })

        })

        .then(res => res.json())

        .then(data => {

            if(data.status === 'ok'){

                const led =
                    document.getElementById('ledIndicator');

                if(state == 1){

                    const currentColor =
                        document.getElementById('colorPicker').value;

                    led.style.background = currentColor;

                    led.style.boxShadow =
                        "0 0 30px " + currentColor;

                }

                else{

                    led.style.background = "#222";

                    led.style.boxShadow =
                        "0 0 10px #000";

                }

            }

        });

    }

    // =========================
    // CHANGE RGB COLOR
    // =========================

    function changeColor(color){

        fetch('/api/color', {

            method:'POST',

            headers:{
                'Content-Type':'application/json'
            },

            body: JSON.stringify({
                color: color
            })

        })

        .then(res => res.json())

        .then(data => {

            if(data.status === 'ok'){

                const led =
                    document.getElementById('ledIndicator');

                led.style.background = color;

                led.style.boxShadow =
                    "0 0 30px " + color;

            }

        });

    }

    // =========================
    // AUTO UPDATE
    // =========================

    setInterval(fetchSensorData,1500);

</script>

</body>
</html>
'''
# --- Flask Routes ---
@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE, esp_ip=ESP32_IP)

@app.route('/api/sensor', methods=['GET'])
def api_sensor():
    return jsonify(get_sensor_data())

@app.route('/api/control', methods=['POST'])
def api_control():
    state = request.json.get('state', 0)
    if control_light(state):
        return jsonify({'status': 'ok'}), 200
    return jsonify({'status': 'error'}), 502

if __name__ == '__main__':
    print(f"[*] Dashboard running. Target ESP32 IP: {ESP32_IP}")
    app.run(host='0.0.0.0', port=5000)