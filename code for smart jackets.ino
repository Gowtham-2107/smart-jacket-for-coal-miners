#include <WiFi.h> 
#include <AsyncTCP.h> 
#include <ESPAsyncWebServer.h> 
#include <Wire.h> 
#include "MAX30105.h" 
#include "heartRate.h" 
#include <MPU6050.h> 
#include <DHT.h> 
#include <OneWire.h> 
#include <DallasTemperature.h> 
#include <NTPClient.h> 
#include <WiFiUdp.h> 
#include <math.h> 
// ---------------- WIFI ---------------- 
const char* ssid = "iotdataa"; 
const char* password = "123456789"; 
// ---------------- THRESHOLDS ---------------- 
#define GAS_THRESHOLD 650 
#define ATM_TEMP_THRESHOLD 40 
48 
#define HUM_THRESHOLD 80 
#define BODY_TEMP_THRESHOLD 38 
#define BPM_LOW 50 
#define BPM_HIGH 120 
#define SPO2_LOW 92 
#define FALL_ANGLE_THRESHOLD 120 
#define BUZZER 2   // You can use GPIO2 (or any free pin) 
// ---------------- SENSORS ---------------- 
MAX30105 particleSensor; 
MPU6050 mpu; 
#define DHTPIN 4 
#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE); 
#define ONE_WIRE_BUS 15 
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature bodyTempSensor(&oneWire); 
#define MQ2_PIN 34; 
// ---------------- SERVER ---------------- 
AsyncWebServer server(80); 
WiFiUDP ntpUDP; 
49 
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 1000); 
// ---------------- VARIABLES ---------------- 
long irValue; 
int bpm = 0, spo2 = 0; 
float atmTemp, humidity, bodyTemp; 
int mq2Value; 
float angleValue; 
bool fallDetected; 
void setup() { 
Serial.begin(115200); 
WiFi.begin(ssid, password); 
while (WiFi.status() != WL_CONNECTED) delay(500); 
Wire.begin(21,22); 
pinMode(BUZZER, OUTPUT); 
digitalWrite(BUZZER, LOW); 
particleSensor.begin(Wire, I2C_SPEED_FAST); 
particleSensor.setup(); 
particleSensor.setPulseAmplitudeRed(0x1F); 
particleSensor.setPulseAmplitudeIR(0x1F); 
50 
mpu.initialize(); 
dht.begin(); 
bodyTempSensor.begin(); 
timeClient.begin(); 
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
request->send(200,"text/html",R"rawliteral( 
<!DOCTYPE html> 
<html> 
<head> 
<meta name="viewport" content="width=device-width, initial-scale=1"> 
<style> 
body{font-family:Arial;background:#eef2f3;padding:10px;} 
h2{text-align:center;} 
.dashboard{display:grid;grid-template-columns:repeat(auto
fit,minmax(180px,1fr));gap:10px;} 
.card{padding:15px;border-radius:10px;text-align:center;font-weight:bold;color:white;} 
.normalCard{background:#2ecc71;} 
.alertCard{background:#e74c3c;} 
</style> 
</head> 
<body> 
51 
<h2>Coal Mine Worker Monitoring</h2> 
<p><b>Time:</b> <span id="datetime"></span></p> 
<div class="dashboard"> 
<div class="card normalCard" id="gasCard">Chemical Gas(MQ135): <span 
id="gas">0</span></div> 
<div class="card normalCard" id="tempCard">Atmospheric Temp: <span 
id="temp">0</span>C</div> 
<div class="card normalCard" id="humCard">Humidity: <span 
id="hum">0</span>%</div> 
<div class="card normalCard" id="bodyCard">Body Temp: <span 
id="body">0</span>C</div> 
<div class="card normalCard" id="bpmCard">BPM: <span id="bpm">0</span></div> 
<div class="card normalCard" id="spo2Card">SpO2(RES): <span 
id="spo2">0</span>%</div> 
<div class="card normalCard" id="angleCard">Body Angle: <span 
id="angle">0</span>&deg;</div> 
</div> 
<script> 
function updateCard(id,alert){ 
let card=document.getElementById(id); 
if(alert){ 
52 
card.classList.remove("normalCard"); 
card.classList.add("alertCard"); 
}else{ 
card.classList.remove("alertCard"); 
card.classList.add("normalCard"); 
} 
} 
setInterval(()=>{ 
fetch('/data').then(res=>res.json()).then(data=>{ 
document.getElementById('datetime').innerText=data.time; 
document.getElementById('gas').innerText=data.gas; 
updateCard("gasCard",data.gasAlert); 
document.getElementById('temp').innerText=data.temp; 
updateCard("tempCard",data.tempAlert); 
document.getElementById('hum').innerText=data.hum; 
updateCard("humCard",data.humAlert); 
document.getElementById('body').innerText=data.body; 
updateCard("bodyCard",data.bodyAlert); 
53 
document.getElementById('bpm').innerText=data.bpm; 
updateCard("bpmCard",data.bpmAlert); 
document.getElementById('spo2').innerText=data.spo2; 
updateCard("spo2Card",data.spo2Alert); 
document.getElementById('angle').innerText=data.angle; 
updateCard("angleCard",data.angleAlert); 
}); 
},1000); 
</script> 
</body> 
</html> 
)rawliteral"); 
}); 
server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){ 
String json="{"; 
json += "\"time\":\""+timeClient.getFormattedTime()+"\","; 
54 
json += "\"gas\":"+String(mq2Value)+","; 
json += "\"gasAlert\":"+String(mq2Value>GAS_THRESHOLD)+","; 
json += "\"temp\":"+String(atmTemp)+","; 
json += "\"tempAlert\":"+String(atmTemp>ATM_TEMP_THRESHOLD)+","; 
json += "\"hum\":"+String(humidity)+","; 
json += "\"humAlert\":"+String(humidity>HUM_THRESHOLD)+","; 
json += "\"body\":"+String(bodyTemp)+","; 
json += "\"bodyAlert\":"+String(bodyTemp>BODY_TEMP_THRESHOLD)+","; 
json += "\"bpm\":"+String(bpm)+","; 
json += "\"bpmAlert\":"+String((bpm<BPM_LOW||bpm>BPM_HIGH))+","; 
json += "\"spo2\":"+String(spo2)+","; 
json += "\"spo2Alert\":"+String((spo2<SPO2_LOW&&spo2!=0))+","; 
json += "\"angle\":"+String(angleValue)+","; 
json += "\"angleAlert\":"+String(angleValue>FALL_ANGLE_THRESHOLD); 
json += "}"; 
55 
request->send(200,"application/json",json); 
}); 
server.begin(); 
} 
void loop() { 
timeClient.update(); 
// MAX30102 (Demo Random Values) 
irValue = particleSensor.getIR(); 
if(irValue>50000){ 
bpm=random(65,95); 
spo2=random(95,99); 
} else { bpm=0; spo2=0; } 
// MPU6050 Angle 0–180° 
int16_t ax,ay,az; 
mpu.getAcceleration(&ax,&ay,&az); 
float Ax=ax/16384.0; 
float Az=az/16384.0; 
56 
float angle=atan2(Ax,Az)*180/PI; 
if(angle<0) angle+=180; 
if(angle>180) angle=180; 
if(angle<0) angle=0; 
angleValue=angle; 
// DHT11 
atmTemp=dht.readTemperature(); 
humidity=dht.readHumidity(); 
if(isnan(atmTemp)) atmTemp=0; 
if(isnan(humidity)) humidity=0; 
// DS18B20 
bodyTempSensor.requestTemperatures(); 
bodyTemp=bodyTempSensor.getTempCByIndex(0); 
// MQ2 
mq2Value=analogRead(34); 
bool gasAlert   = mq2Value > GAS_THRESHOLD; 
bool tempAlert  = atmTemp > ATM_TEMP_THRESHOLD; 
bool humAlert   = humidity > HUM_THRESHOLD; 
57 
bool bodyAlert  = bodyTemp > BODY_TEMP_THRESHOLD; 
bool bpmAlert   = (bpm < BPM_LOW || bpm > BPM_HIGH); 
bool spo2Alert  = (spo2 < SPO2_LOW && spo2 != 0); 
bool fallAlert  = angleValue > FALL_ANGLE_THRESHOLD; 
// If ANY abnormal condition → buzzer ON 
if (gasAlert || tempAlert || humAlert || bodyAlert || bpmAlert || spo2Alert || fallAlert) { 
digitalWrite(BUZZER, HIGH); 
} else { 
digitalWrite(BUZZER, LOW); 
} 
} 
delay(500);