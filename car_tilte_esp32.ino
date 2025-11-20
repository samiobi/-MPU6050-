#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>

// WiFi credentials
const char* ssid = "XXXXXXX";
const char* password = "XXXXXXXX";

// Web server on port 80
ESP8266WebServer server(80);

// Sensor data
float pitch = 0;
float roll = 0;
bool mpuConnected = false;

// MPU6050 I2C address
const int MPU_ADDR = 0x68;

// MPU6050 registers
const int PWR_MGMT_1 = 0x6B;
const int ACCEL_XOUT_H = 0x3B;

// Timing for sensor reads
unsigned long previousMillis = 0;
const long interval = 50;

// HTML page with AIRPLANE COCKPIT visualization
const char* htmlContent = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP8266 MPU6050 Aircraft Attitude Indicator</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { 
      font-family: Arial, sans-serif; 
      text-align: center; 
      background: #1a1a2e;
      margin: 0;
      padding: 20px;
      color: white;
    }
    .container {
      max-width: 600px;
      margin: 0 auto;
      background: #162447;
      padding: 20px;
      border-radius: 15px;
      box-shadow: 0 4px 20px rgba(0,0,0,0.3);
    }
    h1 {
      color: #e43f5a;
      margin-bottom: 20px;
      text-shadow: 0 2px 4px rgba(0,0,0,0.5);
    }
    .status {
      padding: 10px;
      border-radius: 5px;
      margin: 10px 0;
      font-weight: bold;
    }
    .connected {
      background: #1e6f5c;
      color: #fff;
    }
    .disconnected {
      background: #e43f5a;
      color: #fff;
    }
    .cockpit-container {
      position: relative;
      width: 400px;
      height: 400px;
      margin: 20px auto;
      background: #1f4068;
      border: 3px solid #e43f5a;
      border-radius: 50%;
      overflow: hidden;
      box-shadow: inset 0 0 30px rgba(0,0,0,0.5);
    }
    .artificial-horizon {
      position: absolute;
      width: 200%;
      height: 200%;
      top: -50%;
      left: -50%;
      transform-origin: center center;
      transition: transform 0.1s ease;
    }
    .sky {
      position: absolute;
      width: 100%;
      height: 50%;
      background: linear-gradient(to bottom, #a6d8ff, #7bc8ff);
      top: 0;
    }
    .ground {
      position: absolute;
      width: 100%;
      height: 50%;
      background: linear-gradient(to top, #8B4513, #CD853F);
      bottom: 0;
    }
    .horizon-line {
      position: absolute;
      width: 100%;
      height: 3px;
      background: #ffffff;
      top: 50%;
      left: 0;
      transform: translateY(-50%);
      box-shadow: 0 0 10px rgba(255,255,255,0.5);
    }
    .roll-scale {
      position: absolute;
      width: 90%;
      height: 90%;
      top: 5%;
      left: 5%;
      border: 2px solid rgba(255,255,255,0.7);
      border-radius: 50%;
    }
    .aircraft-symbol {
      position: absolute;
      width: 40px;
      height: 40px;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
      z-index: 10;
    }
    .wings {
      position: absolute;
      width: 80px;
      height: 4px;
      background: #000000;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
      box-shadow: 0 0 10px rgba(0,0,0,0.7);
    }
    .fuselage {
      position: absolute;
      width: 4px;
      height: 40px;
      background: #000000;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
      box-shadow: 0 0 10px rgba(0,0,0,0.7);
    }
    .data-display {
      margin: 20px 0;
      font-size: 18px;
      background: rgba(255,255,255,0.1);
      padding: 15px;
      border-radius: 10px;
    }
    .value {
      font-weight: bold;
      color: #e43f5a;
    }
    .instrument-panel {
      display: flex;
      justify-content: space-around;
      margin: 20px 0;
    }
    .instrument {
      background: rgba(0,0,0,0.3);
      padding: 15px;
      border-radius: 10px;
      min-width: 140px;
    }
    .instrument-label {
      font-size: 14px;
      color: #87CEEB;
      margin-bottom: 5px;
    }
    .controls {
      margin: 20px 0;
    }
    .control-btn {
      padding: 10px 20px;
      margin: 5px;
      background: #e43f5a;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-weight: bold;
    }
    .control-btn:hover {
      background: #ff6b81;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1> Attitude Indicator</h1>
    <div id="status" class="status">
      Initializing...
    </div>
    
    <div class="cockpit-container">
      <div class="artificial-horizon" id="horizon">
        <div class="sky"></div>
        <div class="ground"></div>
        <div class="horizon-line"></div>
      </div>
      <div class="roll-scale"></div>
      <div class="aircraft-symbol">
        <div class="wings"></div>
        <div class="fuselage"></div>
      </div>
    </div>
    
    <div class="instrument-panel">
      <div class="instrument">
        <div class="instrument-label">PITCH</div>
        <div class="value" id="pitchValue">0.0°</div>
      </div>
      <div class="instrument">
        <div class="instrument-label">ROLL</div>
        <div class="value" id="rollValue">0.0°</div>
      </div>
    </div>
    
    <div class="controls">
      <button class="control-btn" onclick="resetView()">Reset Attitude</button>
      <button class="control-btn" onclick="calibrate()">Calibrate</button>
    </div>
    
    <div style="margin-top: 20px; color: #87CEEB; font-size: 14px;">
      Aircraft Attitude Indicator | Real-time MPU6050 Data
    </div>
  </div>

  <script>
    let pitchOffset = 0;
    let rollOffset = 0;
    
    function updateData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          const adjustedPitch = data.pitch - pitchOffset;
          const adjustedRoll = data.roll - rollOffset;
          
          document.getElementById('pitchValue').textContent = adjustedPitch.toFixed(1) + '°';
          document.getElementById('rollValue').textContent = adjustedRoll.toFixed(1) + '°';
          
          // Update status
          const statusElement = document.getElementById('status');
          if (data.connected) {
            statusElement.textContent = 'MPU6050: Connected - Live Aircraft Data';
            statusElement.className = 'status connected';
          } else {
            statusElement.textContent = 'MPU6050: Disconnected - Using Simulation';
            statusElement.className = 'status disconnected';
          }
          
          // Update artificial horizon
          const horizon = document.getElementById('horizon');
          // Convert pitch to pixels (3px per degree) and roll to rotation
          const pitchOffsetPx = adjustedPitch * 3;
          horizon.style.transform = `rotate(${adjustedRoll}deg) translateY(${pitchOffsetPx}px)`;
          
        })
        .catch(error => {
          console.log('Error:', error);
        });
    }
    
    function resetView() {
      pitchOffset = 0;
      rollOffset = 0;
    }
    
    function calibrate() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          pitchOffset = data.pitch;
          rollOffset = data.roll;
          alert('Aircraft attitude calibrated! Current position set as level flight.');
        });
    }

    // Start updates
    setInterval(updateData, 100);
    updateData(); // Initial call
  </script>
</body>
</html>
)rawliteral";

// Function to read MPU6050 data
bool readMPU6050(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  
  if (Wire.available() == 6) {
    ax = Wire.read() << 8 | Wire.read();
    ay = Wire.read() << 8 | Wire.read();
    az = Wire.read() << 8 | Wire.read();
    
    // Convert to m/s² (assuming ±2g range)convert raw data to angle 16384 LSB/g sensitivity scale of the accelerometer 
    ax = ax / 16384.0;
    ay = ay / 16384.0;
    az = az / 16384.0;
    
    return true;
  }
  return false;
}

// Initialize MPU6050
bool initMPU6050() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0); // Wake up the MPU-6050
  return Wire.endTransmission() == 0;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nAircraft Attitude Indicator Starting...");
  
  // Initialize I2C
  Wire.begin(D2, D1);
  Wire.setClock(400000); // Set I2C clock to 400kHz
  
  delay(100);
  
  // Initialize MPU6050
  Serial.println("Initializing MPU6050...");
  if (initMPU6050()) {
    Serial.println("MPU6050 initialized successfully!");
    mpuConnected = true;
    
    // Test read
    float ax, ay, az;
    if (readMPU6050(ax, ay, az)) {
      Serial.println("First read successful!");
      Serial.printf("AX: %.2f, AY: %.2f, AZ: %.2f\n", ax, ay, az);
    } else {
      Serial.println("First read failed!");
      mpuConnected = false;
    }
  } else {
    Serial.println("MPU6050 initialization failed!");
    mpuConnected = false;
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  
  // Set up web server routes
  server.on("/", []() {
    server.send(200, "text/html", htmlContent);
  });
  
  server.on("/data", []() {
    String json = "{\"pitch\":" + String(pitch, 2) + 
                  ",\"roll\":" + String(roll, 2) + 
                  ",\"connected\":" + String(mpuConnected ? "true" : "false") + "}";
    server.send(200, "application/json", json);
  });
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Open browser and go to: http://" + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();
  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    if (mpuConnected) {
      // Read real sensor data
      float ax, ay, az;
      if (readMPU6050(ax, ay, az)) {
        // Calculate pitch and roll from accelerometer data
        // Pitch (rotation around Y-axis) - nose up/down
        pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
        
        // Roll (rotation around X-axis) - wing up/down
        roll = atan2(ay, az) * 180.0 / PI;
        
        // Apply simple low-pass filter for smoothing
        static float pitchFiltered = 0;
        static float rollFiltered = 0;
        float alpha = 0.3; // Smoothing factor
        
        pitchFiltered = pitchFiltered * (1 - alpha) + pitch * alpha;
        rollFiltered = rollFiltered * (1 - alpha) + roll * alpha;
        
        pitch = pitchFiltered;
        roll = rollFiltered;
        
        Serial.printf("Aircraft Attitude - Pitch: %.2f°, Roll: %.2f°\n", pitch, roll);
      } else {
        Serial.println("Failed to read MPU6050 data!");
        mpuConnected = false;
      }
    } else {
      // Generate demo data when sensor is not connected
      static float demoCounter = 0;
      demoCounter += 0.05;
      pitch = sin(demoCounter) * 20; // ±20 degrees pitch
      roll = cos(demoCounter * 0.7) * 30; // ±30 degrees roll
    }
  }
}
