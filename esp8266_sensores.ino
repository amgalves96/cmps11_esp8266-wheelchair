#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <iostream>
#include <string.h>
#include <Wire.h>
#define CMPS11_ADDRESS 0x60  // Address of CMPS11 shifted right one bit for arduino wire library
#define ANGLE_8  1           // Register to read 8bit angle from

// Wifi
const char* ssid = "TP-LINK_1456";       // Wi-Fi name
const char* password = "85265998";   // Wi-Fi password
    
// MQTT Broker
const char* mqtt_server = "192.168.0.107";
const int mqtt_port = 1883;

// CMPS11
unsigned char high_byte, low_byte, angle8;
char pitch, roll;
unsigned int angle16;
float angle;

WiFiClient espClient;
PubSubClient MQTT(espClient);

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconectWiFi() 
{
    //se já está conectado a rede WI-FI, nada é feito. 
    //Caso contrário, são efetuadas tentativas de conexão
    if (WiFi.status() == WL_CONNECTED)
        return;
         
    WiFi.begin(ssid, password); // Conecta na rede WI-FI
     
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(100);
        Serial.print(".");
    }
   
    Serial.println();
    Serial.print("Conectado com sucesso na rede ");
    Serial.print(ssid);
    Serial.println("IP obtido: ");
    Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String msg;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    msg += (char)message[i];
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!MQTT.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (MQTT.connect("ESP8266")) {
      Serial.println("connected");
      // Subscribe
      MQTT.subscribe("testTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(MQTT.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void Heading_loop() //Below gives heading data in Serial Monitor, uncomment in loop() to use
{

  Wire.beginTransmission(CMPS11_ADDRESS);  //starts communication with CMPS11
  Wire.write(ANGLE_8);                     //Sends the register we wish to start reading from
  Wire.endTransmission();
 
  // Request 5 bytes from the CMPS11
  // this will give us the 8 bit bearing, 
  // both bytes of the 16 bit bearing, pitch and roll
  Wire.requestFrom(CMPS11_ADDRESS, 5);       
  
  while(Wire.available() < 5);        // Wait for all bytes to come back
  
  angle8 = Wire.read();               // Read back the 5 bytes
  high_byte = Wire.read();
  low_byte = Wire.read();
  pitch = Wire.read();
  roll = Wire.read();
  
  angle16 = high_byte;                 // Calculate 16 bit angle
  angle16 <<= 8;
  angle16 += low_byte;
  
  float curr_angle = (String(int(angle16/10)) + "." +  String(angle16%10)).toFloat();
  int curr_roll = int(roll);
  int curr_pitch = int(pitch);

  char msg[20];
  if(curr_roll > 177){
    curr_roll = curr_roll - 256;
  }
  if(curr_pitch > 177){
    curr_pitch = curr_pitch - 256;
  }
  (String(curr_roll) + "," + String(curr_pitch) + "," + String(curr_angle)).toCharArray(msg, 20);
  MQTT.publish("/gyro", msg);
    
  Serial.print("roll: ");               // Display roll data
  Serial.print(curr_roll);
  
  Serial.print("    pitch: ");          // Display pitch data
  Serial.print(curr_pitch);
  
  Serial.print("    YAW: ");     // Display 16 bit angle with decimal place
  Serial.println(curr_angle);
  
   
  delay(100);                           // Short delay before next loop,(100) 10hz update
}

void calibrate()//Below used to do manual mag/accel calibration, uncomment in setup to use
{

  Serial.println(" Calibration Mode ");
  delay(2000);  //2 second before starting
  Serial.println(" Start ");

  Wire.beginTransmission(CMPS11_ADDRESS);
  Wire.write(0); //command register
  Wire.write(0xF0);
  Wire.endTransmission();
  delay(25);

  Wire.beginTransmission(CMPS11_ADDRESS);
  Wire.write(0); //command register
  Wire.write(0xF5);
  Wire.endTransmission();
  delay(25);

  Wire.beginTransmission(CMPS11_ADDRESS);
  Wire.write(0); //command register
  Wire.write(0xF6);
  Wire.endTransmission();

  delay(180000);//(ms)Alter this to allow enough time for you to calibrate Mag and Accel

  Wire.beginTransmission(CMPS11_ADDRESS);
  Wire.write(0); //command register
  Wire.write(0xF8);
  Wire.endTransmission();
  Serial.println(" done ");
  
}

void default_calibrate()//Below resets factory default cailbration, to use uncomment in setup loop
{

  Serial.println(" Default Calibration Mode ");
  delay(2000);  //2 second before starting
  Serial.println("Start");

  Wire.beginTransmission(CMPS11_ADDRESS);
  Wire.write(0); //command register
  Wire.write(0x6A);
  Wire.endTransmission();
  delay(25);

  Wire.beginTransmission(CMPS11_ADDRESS);
  Wire.write(0); //command register
  Wire.write(0x7C);
  Wire.endTransmission();
  delay(25);

  Wire.beginTransmission(CMPS11_ADDRESS);
  Wire.write(0); //command register
  Wire.write(0xb1);
  Wire.endTransmission();

 
  Serial.println("done");

}


void setup() {
  Serial.begin(9600);

  Wire.begin();
  default_calibrate();// Uncomment this line to reset IMU back to default cal data, uncomment only 1
  setup_wifi();

  delay(100);
  
  MQTT.setServer(mqtt_server, mqtt_port);   //informa qual broker e porta deve ser conectado
  MQTT.setCallback(callback);            //atribui função de callback (função chamada quando qualquer informação de um dos tópicos subescritos chega)

}

void loop() {
  if (!MQTT.connected()) {
        reconnect(); //se não há conexão com o Broker, a conexão é refeita
  }
  reconectWiFi();

  // CODE HERE
  Heading_loop();// Comment out to do either Cal modes, uncomment  to display heading, uncomment only 1

  //char msg[8] = "Cenas";
  //MQTT.publish("/gyro", msg);

  MQTT.loop();
}
