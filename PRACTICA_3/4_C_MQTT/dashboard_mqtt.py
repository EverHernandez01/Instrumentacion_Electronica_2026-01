"""
IoT RGB LED Dashboard - MQTT + Flask
Control de LED RGB desde navegador usando MQTT
"""

from flask import Flask, render_template_string, request, jsonify
import paho.mqtt.client as mqtt
import json
import logging

app = Flask(__name__)

log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)

# =========================
# MQTT CONFIG
# =========================
MQTT_BROKER = "localhost"
MQTT_PORT = 1883

TOPIC_CONTROL = "iot/control"

# =========================
# MQTT CLIENT
# =========================
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"[MQTT] Connected to broker (rc={reason_code})")

mqtt_client.on_connect = on_connect

# =========================
# HTML DASHBOARD
# =========================
HTML_TEMPLATE = '''
<!DOCTYPE html>
<html lang="en">

<head>

    <meta charset="UTF-8">

    <meta name="viewport"
          content="width=device-width, initial-scale=1.0">

    <title>ESP32 RGB LED Controller</title>

    <style>

        *{
            margin:0;
            padding:0;
            box-sizing:border-box;
        }

        body{

            background:#0f172a;

            font-family:Arial, sans-serif;

            display:flex;

            justify-content:center;

            align-items:center;

            min-height:100vh;

            color:white;
        }

        .card{

            width:450px;

            background:#1e293b;

            padding:35px;

            border-radius:20px;

            box-shadow:0 0 30px rgba(0,0,0,0.45);
        }

        h1{

            text-align:center;

            margin-bottom:10px;

            font-size:32px;
        }

        .subtitle{

            text-align:center;

            color:#94a3b8;

            margin-bottom:25px;
        }

        .preview{

            width:170px;

            height:170px;

            border-radius:50%;

            margin:20px auto;

            background:#ff0000;

            border:5px solid white;

            transition:0.2s;
        }

        .picker-container{

            text-align:center;

            margin-top:20px;
        }

        input[type="color"]{

            width:120px;

            height:60px;

            border:none;

            cursor:pointer;

            background:none;
        }

        .rgb-values{

            margin-top:20px;

            text-align:center;

            font-size:20px;

            color:#cbd5e1;
        }

        .preset-title{

            margin-top:30px;

            margin-bottom:15px;

            text-align:center;

            font-size:18px;

            font-weight:bold;
        }

        .preset-colors{

            display:grid;

            grid-template-columns:repeat(4,1fr);

            gap:15px;

            margin-top:10px;
        }

        .color-btn{

            width:75px;

            height:75px;

            border-radius:15px;

            cursor:pointer;

            border:3px solid white;

            transition:0.2s;
        }

        .color-btn:hover{

            transform:scale(1.08);
        }

        .buttons{

            display:flex;

            gap:15px;

            margin-top:35px;
        }

        button{

            flex:1;

            padding:16px;

            border:none;

            border-radius:12px;

            font-size:16px;

            font-weight:bold;

            cursor:pointer;

            transition:0.2s;
        }

        button:hover{

            transform:scale(1.03);
        }

        .on-btn{

            background:#22c55e;

            color:white;
        }

        .off-btn{

            background:#ef4444;

            color:white;
        }

    </style>

</head>

<body>

<div class="card">

    <h1>ESP32 RGB LED</h1>

    <div class="subtitle">

        MQTT RGB Controller

    </div>

    <div class="preview"
         id="preview">
    </div>

    <div class="picker-container">

        <input type="color"
               id="colorPicker"
               value="#ff0000">

    </div>

    <div class="rgb-values"
         id="rgbText">

         R:255 G:0 B:0

    </div>

    <div class="preset-title">

        Quick Colors

    </div>

    <div class="preset-colors">

        <div class="color-btn"
             style="background:#ff0000"
             onclick="setPresetColor('#ff0000')">
        </div>

        <div class="color-btn"
             style="background:#00ff00"
             onclick="setPresetColor('#00ff00')">
        </div>

        <div class="color-btn"
             style="background:#0000ff"
             onclick="setPresetColor('#0000ff')">
        </div>

        <div class="color-btn"
             style="background:#ffff00"
             onclick="setPresetColor('#ffff00')">
        </div>

        <div class="color-btn"
             style="background:#ff00ff"
             onclick="setPresetColor('#ff00ff')">
        </div>

        <div class="color-btn"
             style="background:#00ffff"
             onclick="setPresetColor('#00ffff')">
        </div>

        <div class="color-btn"
             style="background:#ffffff"
             onclick="setPresetColor('#ffffff')">
        </div>

        <div class="color-btn"
             style="background:#ff8800"
             onclick="setPresetColor('#ff8800')">
        </div>

    </div>

    <div class="buttons">

        <button class="on-btn"
                onclick="sendLED(1)">

            ON

        </button>

        <button class="off-btn"
                onclick="sendLED(0)">

            OFF

        </button>

    </div>

</div>

<script>

    const picker =
        document.getElementById("colorPicker");

    const preview =
        document.getElementById("preview");

    const rgbText =
        document.getElementById("rgbText");

    function hexToRgb(hex){

        let r =
            parseInt(hex.substring(1,3),16);

        let g =
            parseInt(hex.substring(3,5),16);

        let b =
            parseInt(hex.substring(5,7),16);

        return {r,g,b};
    }

    function updatePreview(){

        const color = picker.value;

        preview.style.background = color;

        const rgb = hexToRgb(color);

        rgbText.innerHTML =
            `R:${rgb.r} G:${rgb.g} B:${rgb.b}`;
    }

    picker.addEventListener(
        "input",
        updatePreview
    );

    function setPresetColor(color){

        picker.value = color;

        updatePreview();
    }

    updatePreview();

    function sendLED(state){

        const rgb =
            hexToRgb(picker.value);

        fetch('/api/control', {

            method:'POST',

            headers:{
                'Content-Type':'application/json'
            },

            body:JSON.stringify({

                state:state,

                r:rgb.r,

                g:rgb.g,

                b:rgb.b
            })

        })
        .then(res => res.json())
        .then(data => {

            if(data.status !== 'ok'){

                alert("MQTT Publish Error");
            }
        });
    }

</script>

</body>

</html>
'''
# =========================
# FLASK ROUTES
# =========================

@app.route('/')
def index():

    return render_template_string(
        HTML_TEMPLATE,
        broker=MQTT_BROKER,
        port=MQTT_PORT
    )

@app.route('/api/control', methods=['POST'])
def api_control():

    data = request.json

    payload = json.dumps({
        "state": data.get("state", 0),
        "r": data.get("r", 0),
        "g": data.get("g", 0),
        "b": data.get("b", 0)
    })

    result = mqtt_client.publish(
        TOPIC_CONTROL,
        payload,
        qos=1
    )

    if result.rc == mqtt.MQTT_ERR_SUCCESS:

        print(f"[MQTT] Published: {payload}")

        return jsonify({'status':'ok'}), 200

    return jsonify({
        'status':'error'
    }), 500

# =========================
# MAIN
# =========================
if __name__ == '__main__':

    mqtt_client.connect(
        MQTT_BROKER,
        MQTT_PORT,
        keepalive=60
    )

    mqtt_client.loop_start()

    print("[*] RGB LED Dashboard Running")
    print(f"[*] Broker: {MQTT_BROKER}:{MQTT_PORT}")

    app.run(
        host='0.0.0.0',
        port=5000
    )