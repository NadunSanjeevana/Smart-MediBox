#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

#define DHT_PIN 15
#define BUZZER 12
#define LDR A0
#define MOTOR 18

char tempAr[6];

bool isScheduledON = false;
unsigned long scheduledOnTime;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

DHTesp dhtSensor;
Servo servo;

const float gama = 0.7;
const float rl10 = 50;
int angle = 0;
int Angle_Offset = 30;
float lux = 0.5;
float Controlling_Factor = 0.75;




void setup() {
  Serial.begin(115200);
  setupWifi();

  setupMqtt();

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  timeClient.begin();
  timeClient.setTimeOffset(5.5*3600);

  pinMode(BUZZER,OUTPUT);
  digitalWrite(BUZZER,LOW);
  pinMode(LDR, INPUT);
  pinMode(MOTOR, OUTPUT);

  servo.attach(MOTOR, 500, 2400);

}

void loop() {

  if(!mqttClient.connected()){
    connectToBrocker();
  }
  mqttClient.loop();

  updateTemperature();
  Serial.println(tempAr);
  mqttClient.publish("ADMIN-TEMP",tempAr);

  checkSchedule();
  updateLightIntensity();
  rotateMotor();
  mqttClient.publish("ADMIN-LIGHT-INTENSITY",tempAr);

  delay(1000);

}

void buzzerOn(bool on){
  if(on){
    tone(BUZZER,256);
  }else{
    noTone(BUZZER);
  }

}

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(receiveCallback);
}

void receiveCallback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message arrived");
  Serial.print(topic);
  Serial.print("]");

  char payloadCharAr[length];
  for(int i = 0; i < length; i++){
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }

  Serial.println();

 if (strcmp(topic,"ADMIN-MAIN-ON-OFF")==0){  
      buzzerOn(payloadCharAr[0]=='1');
  }
  else if (strcmp(topic,"ADMIN-SCH-ON")==0){
      if (payloadCharAr[0]=='N'){
        isScheduledON = false;
      }else if(payloadCharAr[0]=='1'){
        isScheduledON = true;
        scheduledOnTime = atol(payloadCharAr);
      }
  }
  if(strcmp(topic, "ADMIN-ANGLE-OFFSET") == 0){
    Angle_Offset = atoi(payloadCharAr);
    Serial.println(Angle_Offset);
  }
  if(strcmp(topic, "ADMIN-CONTROLLING_FACTOR") == 0){
    Controlling_Factor = atof(payloadCharAr);
    Serial.println(Controlling_Factor);
  }
}


void connectToBrocker(){
  while(!mqttClient.connected()){
    Serial.print("Attempting MQTT connection ...");
    if(mqttClient.connect("ESP32-123456789")){
      Serial.println("connected");
      mqttClient.subscribe("ADMIN-MAIN-ON-OFF");
      mqttClient.subscribe("ADMIN-SCH-ON");
       mqttClient.subscribe("ADMIN-ANGLE-OFFSET");
      mqttClient.subscribe("ADMIN-CONTROLLING_FACTOR");
    }
    else{
      Serial.print("failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void updateTemperature(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature,2).toCharArray(tempAr,6);
}

void setupWifi(){
  Serial.println("Connecting to ");
  Serial.println("Wokwi-GUEST");
  WiFi.begin("Wokwi-GUEST", "");

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi Connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

unsigned long getTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}


void checkSchedule(){
  if(isScheduledON){
    unsigned long currentTime = getTime();
    if(currentTime > scheduledOnTime){
      buzzerOn(true);
      isScheduledON = false;
      mqttClient.publish("ADMIN-MAIN-ON-OFF-ESP","1");
      mqttClient.publish("ADMIN-SCH-ESP-ON","0");
      Serial.println("Scheduled ON");
    }else{
      buzzerOn(false);
    }
  }
}

void updateLightIntensity(){
  int analogValue = analogRead(LDR);
  float voltage = analogValue / 1024.  * 5;
  float resistance = 2000 * voltage / (1 - voltage / 5);
  lux = pow (rl10 * 1e3 * pow(10,gama) / resistance, (1 / gama))/13460;
  // Serial.println( adcValue);
  String(lux).toCharArray(tempAr,6);
  Serial.println(lux);
}

void rotateMotor(){
  angle = Angle_Offset + (180 - Angle_Offset) * lux * Controlling_Factor;
  servo.write(angle);
  delay(15);

}

