#include <WiFi.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WebServer.h>
#include <SPIFFS.h>  // File system for storing JSON

// String video_url = "";

// WiFi Credentials
#define WIFI_SSID "Redmi Note 13 Pro 5G"
#define WIFI_PASSWORD "jojo1234"


// GPS Module Pins
#define RXD2 16        // GPS TX → ESP32 RX2
#define TXD2 17        // GPS RX → ESP32 TX2
#define GPS_BAUD 9600  // Standard GPS baud rate


// Touch Module pins
#define TOUCH_PIN 4  // TTP223B SIG connected to GPIO 4

// ECG Module Pins
#define ECG_PIN 34   // ECG signal input (GPIO34)
#define LO_PLUS 32   // Electrode Status Pin (GPIO32)
#define LO_MINUS 33  // Electrode Status Pin (GPIO33)

// LM35 Module Pins
#define LM35_PIN 36  // LM35 VOUT connected to GPIO 34


// Create GPS, MPU6050, TTP223B and Web Server objects
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
Adafruit_MPU6050 mpu;
WebServer server(80);
bool touchDetected = false;


// Global variables for storing GPS data
double latitude = 0.0, longitude = 0.0;
String gpsTime = "N/A", gpsDate = "N/A";
// GPS Update Interval
unsigned long lastGPSUpdate = 0;                  // Stores last update time
const unsigned long GPS_UPDATE_INTERVAL = 30000;  // 15-second interval
// Function to read GPS data
void updateGPS() {
  if (millis() - lastGPSUpdate >= GPS_UPDATE_INTERVAL) {  // Check if 15s passed
    lastGPSUpdate = millis();                             // Update timestamp

    while (gpsSerial.available() > 0) {
      char c = gpsSerial.read();
      if (gps.encode(c)) {
        if (gps.location.isValid()) {
          latitude = gps.location.lat();
          longitude = gps.location.lng();
        }
        if (gps.date.isValid()) {
          gpsDate = String(gps.date.month()) + "/" + String(gps.date.day()) + "/" + String(gps.date.year());
        }
        if (gps.time.isValid()) {
          gpsTime = String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second());
        }
      }
    }
  }
}
// Serve GPS Data as JSON
void handleGPSData() {
  updateGPS();
  String json = "{";
  json += "\"latitude\": " + String(latitude, 6) + ",";
  json += "\"longitude\": " + String(longitude, 6) + ",";
  json += "\"date\": \"" + gpsDate + "\",";
  json += "\"time\": \"" + gpsTime + "\"";
  json += "}";
  server.send(200, "application/json", json);
}






// 📌 Serve MPU6050 Sensor Data as JSON
void handleSensorData() {
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);

    // 📌 Adjust Temperature (Subtract 3°C)
    float adjustedTemp = temp.temperature - 0;

    // 📌 Movement & Posture Detection
    String movement, posture;
    const float gyroThreshold = 1.0;  // Adjusted gyroscope threshold
    const float accelThreshold = 0.5; // Adjusted acceleration threshold
    

    // Calculate absolute values for better comparison
float absGyroX = abs(gyro.gyro.x);
float absGyroY = abs(gyro.gyro.y);
float absGyroZ = abs(gyro.gyro.z);
float absAccelX = abs(accel.acceleration.x);
float absAccelY = abs(accel.acceleration.y);
float absAccelZ = abs(accel.acceleration.z - 9.81); // Subtract gravity

// Check if the soldier is rotating
bool isRotating = (absGyroX > gyroThreshold || absGyroY > gyroThreshold || absGyroZ > gyroThreshold);

// Check if the soldier is moving (considering gravity)
bool isMoving = (absAccelX > accelThreshold);

if (isMoving) {
    movement = "Moving";
} else {
    movement = "Stationary";
}

    // 📌 Determine Posture (Standing, Lying Down, or Crouching)
    if (abs(accel.acceleration.z) > 9.0 && abs(accel.acceleration.x) > 0.5 && abs(accel.acceleration.y) > 0.5 ) {
        posture = "Standing";  // Mainly Z-axis acceleration
    } 
    else if (abs(accel.acceleration.x) > 9.0 && abs(accel.acceleration.y) > 0.1 && abs(accel.acceleration.z) > 0.5 ) {
        posture = "Lying Down";  // Mainly X-axis acceleration
    } 
    else if (abs(accel.acceleration.z) > 5.0 && abs(accel.acceleration.z) < 9.0 && abs(accel.acceleration.x) > 1.0 && abs(accel.acceleration.y) > 1.0) {
        posture = "Crouching";  // Lower Z acceleration, with some X and Y motion
    } 
    else {
        posture = "Standing";  // Default posture
    }


    // 📌 Construct JSON Response
    String json = "{";
    json += "\"ax\":" + String(accel.acceleration.x) + ",";
    json += "\"ay\":" + String(accel.acceleration.y) + ",";
    json += "\"az\":" + String(accel.acceleration.z) + ",";
    json += "\"gx\":" + String(gyro.gyro.x) + ",";
    json += "\"gy\":" + String(gyro.gyro.y) + ",";
    json += "\"gz\":" + String(gyro.gyro.z) + ",";
    json += "\"temp\":" + String(adjustedTemp) + ",";
    json += "\"movement\":\"" + movement + "\",";
    json += "\"posture\":\"" + posture + "\"";
    json += "}";

    server.send(200, "application/json", json);
}



// 📌 Initialize SPIFFS to store JSON data
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Initialization Failed!");
    return;
  }
  Serial.println("SPIFFS Initialized.");
}



// 📌 Save touch status to JSON file
void saveTouchStatus(bool status) {
  File file = SPIFFS.open("/touch_data.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing.");
    return;
  }
  String json = "{ \"touch\": " + String(status ? "true" : "false") + " }";
  file.print(json);
  file.close();
}


// 📌 Read ECG Data Directly
int readECGSignal() {
  return analogRead(ECG_PIN);
}






// Function to read temperature from LM35
float getTemperature() {
  int adcValue = analogRead(LM35_PIN);
  float voltage = adcValue * (3.3 / 4095.0);
  float temperature = (voltage * 100.0) + 11;

  if (temperature < 15) {
    return 0;  
  }

  return temperature;
}













// Serve Web Page with Google Maps, MPU6050, Touch Sensor & ECG Graph
void handleRoot() {
  String html = R"rawliteral(
    
<!DOCTYPE html>
<html>

<head>
    <title>S.H.P.M</title>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.7.0/chart.min.js"></script>
    <script src="https://maps.googleapis.com/maps/api/js?key= <Google Api Key> "></script>
    <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>

    <script>

        function updateData() {
            fetch('/gps')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('gps').innerText = "Lat: " + data.latitude + ", Lng: " + data.longitude;
                    if (data.latitude && data.longitude) {
                        var map = new google.maps.Map(document.getElementById('map'), {
                            center: { lat: parseFloat(data.latitude), lng: parseFloat(data.longitude) },
                            zoom: 18
                        });
                        new google.maps.Marker({
                            position: { lat: parseFloat(data.latitude), lng: parseFloat(data.longitude) },
                            map: map,
                            title: 'Soldier Location'
                        });
                    }
                });





            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('temp').innerText = data.temp.toFixed(2);
                    document.getElementById('ax').innerText = data.ax.toFixed(2);
                    document.getElementById('ay').innerText = data.ay.toFixed(2);
                    document.getElementById('az').innerText = data.az.toFixed(2);
                    document.getElementById('gx').innerText = data.gx.toFixed(2);
                    document.getElementById('gy').innerText = data.gy.toFixed(2);
                    document.getElementById('gz').innerText = data.gz.toFixed(2);


                    document.getElementById('movement').innerText = data.movement;
                    
                    document.getElementById('posture').innerText = data.posture;                  






                });




            fetch('/touch')
                .then(response => response.json())
                .then(data => {
                    let status = document.getElementById('status');
                    let indicator = document.getElementById('indicator');
                    if (data.touch) {
                        status.innerText = "NEED IMMEDIATE BACKUP";
                        status.style.color = "#ff0000";
                        indicator.style.backgroundColor = "#ff0000"; // Red light ON
                    } else {
                        status.innerText = "MISSION IS RUNNING GOOD";
                        status.style.color = "black";
                        indicator.style.backgroundColor = "green"; // Red light OFF
                    }
                });



          setTimeout(updateData, 2000);
            
        }






        let ecgData = [], labels = [];
        for (let i = 0; i < 100; i++) { ecgData.push(0); labels.push(i); }
        function updateECG() {
          console.log("Fetching ECG data...");  
            fetch('/ecg')
                .then(response => response.json())
                .then(data => {
                  console.log("ECG Value: ", data.ecg);  
                    ecgData.shift();
                    ecgData.push(data.ecg);
                    chart.update();
                })
                .catch(error => console.log("ECG Fetch Error: " + error));
          
          
          setTimeout(updateECG, 100);
        }




        window.onload = function() {
          updateData();
          setTimeout(updateECG, 1000);  // Start ECG after delay to ensure chart exists
        };
        
       






        function reloadImage() {
            document.getElementById('stream').src = 'http://192.0.0.4:8080//video?t=' + new Date().getTime();
        }
        setInterval(reloadImage, 1000); // Reload every second



        function updateTemperature() {
            fetch('/temperature')
                .then(response => response.text())
                .then(temp => {
                    document.getElementById('tempValue').innerHTML = temp + ' °C';
                });
        }
        setInterval(updateTemperature, 1000);






    </script>

</head>

<body style="font-family: Arial, sans-serif; text-align: center; background: #808bbb;">

    <!-- HEADING Section -->
    <h2 style="color: Black; border-bottom: 3px solid black; display: inline-block;">Soldier Health & Position Monitoring System</h2>

    <div style="display: flex; justify-content: center; align-items: center; gap: 20px; padding-bottom: 10px;">


        <div
            style="border: 3px solid black; padding: 20px; border-radius: 15px; background: white; display: flex; gap: 20px;">
            <h3>GPS Location: <span id="gps">Loading...</span> </h3>
        </div>



        <div
            style="border: 3px solid black; padding: 20px; border-radius: 15px; background: white; display: flex; gap: 20px;">
            <h3>Body Temperature: <span id='tempValue'>Loading...</span></h3>
        </div>


        <div
            style="border: 3px solid black; padding: 20px; border-radius: 15px; background: white; display: flex; gap: 20px;">
            <h3>Surrounding Temperature:  <b><span id="temp">Loading...</span> °C</b></h3>                                
        </div>


        <div
            style="border: 3px solid black; padding: 20px; border-radius: 15px; background: white; display: flex; gap: 20px;">
            <h4> Status: <span id="status" style="font-weight: bold;">Checking...</span></h4>
            <span id="indicator"
                style="width:50px; height:50px; background-color:gray; margin:auto; border-radius:50%;"></span>
        </div>



    </div>

    <!-- MAP Section -->
    <div
        style="border: 3px solid black; padding: 20px; border-radius: 15px; background: white; display: flex; gap: 20px; ">

        <div id="map" style="width: 80%; height:400px; margin:auto; border: 3px solid black; border-radius: 15px"></div>

        <div
            style="width:50%; height:400px; margin:auto; border: 3px solid black; border-radius: 15px; display: flex; ">
            <img id="stream" style="border-radius: 10px" src="http://192.0.0.4:8080//video" alt="Live Camera Feed">
        </div>

    </div>

    <!-- <h2 style="color: #333;">ESP32 MPU6050 Sensor Dashboard</h2>  -->

    <div style="display: flex; justify-content: center; align-items: center; gap: 20px; padding-top: 20px;">

        <!-- GYRO, ACCELERATION Box -->
        <div style="border: 3px solid black; padding: 20px; border-radius: 15px; background: white; display: flex; gap: 20px;  width: 700px; height: 350px">

            <!-- Movement & Posture Section -->
            <div
                style="  border: 2px solid black; padding: 15px; width: 700px; text-align: center; background: #FFFFFF; border-radius: 10px;">
                <h2 style="border-bottom: 3px solid black; display: inline-block;">Movement and Posture</h2>
                <p> <h3> <b>Movement:</b> </h3> <h2> <span id="movement">loading...</span></h2>  </p>
                <p> <h3> <b>Posture:</b> </h3> <h2> <span id="posture">loading...</span></h2>  </p>
            </div>



        </div>

        <div style="border: 3px solid black; padding: 20px; border-radius: 15px; background: white; display: flex; gap: 20px;   width: 700px; height: 350px">

            <canvas id='ecgChart' ></canvas>
            <script>
                let ctx = document.getElementById('ecgChart').getContext('2d');
                let chart = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels: labels,
                        datasets: [{
                            label: 'ECG Signal',
                            data: ecgData,
                            borderColor: 'red',
                            borderWidth: 2,
                            fill: false,
                            tension: 0.1
                        }]
                    },
                    options: {
                        responsive: true,
                        scales: {
                            x: { display: false },
                            y: { min: 0, max: 4095 }
                        }
                    }
                });
            </script>

        </div>





    </div>

            <!-- Gyroscope Section -->
            <div style="border: 2px solid black; padding: 15px; width: 200px; text-align: center; justify-items: center; background: #add8e6; border-radius: 10px; display: none;">
                <h3>Gyroscope</h3>
                <p><b>X:</b> <span id="gx">0</span> rad/s</p>
                <p><b>Y:</b> <span id="gy">0</span> rad/s</p>
                <p><b>Z:</b> <span id="gz">0</span> rad/s</p>
            </div>

            <!-- Acceleration Section -->
            <div
                style="border: 2px solid black; padding: 15px; width: 200px; text-align: center; background: #90ee90; border-radius: 10px; display: none;">
                <h3>Acceleration</h3>
                <p><b>X:</b> <span id="ax">0</span> m/s²</p>
                <p><b>Y:</b> <span id="ay">0</span> m/s²</p>
                <p><b>Z:</b> <span id="az">0</span> m/s²</p>
            </div>
</body>

</html>
    )rawliteral";

  server.send(200, "text/html", html);
}










// 📌 Serve Touch Sensor Data as JSON
void handleTouchStatus() {
  int touchState = digitalRead(TOUCH_PIN);
  touchDetected = (touchState == HIGH);

  saveTouchStatus(touchDetected);  // Save status to JSON file

  String json = "{ \"touch\": " + String(touchDetected ? "true" : "false") + " }";
  server.send(200, "application/json", json);
}


// 📌 Serve ECG Data
void handleECG() {
   int ecgValue = analogRead(ECG_PIN);  // Read ECG value

    Serial.print("ECG Value: ");  // Debugging
    Serial.println(ecgValue); 

    String json = "{ \"ecg\": " + String(ecgValue) + " }";



  // String json = "{ \"ecg\": " + String(readECGSignal()) + " }";
  server.send(200, "application/json", json);
}




// Handle temperature request for AJAX
void handleTemperature() {
  server.send(200, "text/plain", String(getTemperature(), 2));
}





void setup() {
  Serial.begin(115200);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(LO_PLUS, INPUT);
  pinMode(LO_MINUS, INPUT);
  pinMode(ECG_PIN, INPUT);
  analogReadResolution(12);  // Set ADC to 12-bit mode (0-4095)

  // Initialize SPIFFS for storing JSON
  initSPIFFS();


  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! IP Address: " + WiFi.localIP().toString());

  // Initialize MPU6050
  Wire.begin();
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip!");
    while (1)
      ;
  }
  Serial.println("MPU6050 Initialized!");

  // Define Web Server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/gps", HTTP_GET, handleGPSData);
  server.on("/data", HTTP_GET, handleSensorData);
  server.on("/touch", handleTouchStatus);
  server.on("/ecg", handleECG);
  server.on("/", handleRoot);                    // Serve HTML page
  server.on("/temperature", handleTemperature);  // Serve temperature data
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}
