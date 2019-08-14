/*
 Basic ESP32 MQTT example

 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP32 board/library.

*/
#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include "Wire.h"
#include "SSD1306.h" 

#define BUILTIN_LED 4
#define SENSOR_TH 13
#define SDA 2
#define SCL 14

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// Update these with values suitable for your network.
const char* ssid = "Nightgost";
const char* password = "h18515062134y";
const char* mqtt_server = "192.168.1.100";
String CLIENT_ID = "esp32-001";
long lastMsg = 0;
char msg[50];
int value = 0;

//String getTemperature();
//void rcvMsgHandler(String msg);
void startCameraServer();
SSD1306  display(0x3c, SDA, SCL);

WiFiClient espClient;
PubSubClient client(espClient);
DHTesp dht;

void setupOled() {
  Wire.begin(SDA, SCL);
  display.init();
  display.clear();
  display.drawString(4, 12, "Welcome To:");
  display.drawString(8, 36, "HelloWorld ~");
  display.display();
  delay(1000);
}

void setupWifi() {
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

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  // Handle received massage
  Serial.println(msg);
  rcvMsgHandler(msg);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a client ID
    //String clientId = CLIENT_ID;
    //clientId += String(random(0xffff), HEX);
 
    if (client.connect(CLIENT_ID.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      String msg = "Im " + CLIENT_ID;
      client.publish("out", msg.c_str());
      // ... and resubscribe
      client.subscribe(CLIENT_ID.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void rcvMsgHandler(String msg){
  // Update sensors data
  Serial.println("RemoteMsg: " + msg);
  display.clear();
  display.drawString(2, 2, "MQTT Test:");
  display.drawString(2, 20, "Msg: " + msg);
  display.display();

  if (msg.equals("sensor")){
    String tempHumi="";
    tempHumi = getTempHumi();
    client.publish("out", tempHumi.c_str());
    display.clear();
    display.drawString(2, 2, "MQTT Test:");
    display.drawString(0, 20, tempHumi);
    display.display();
  }
  // Camera
  else if (msg.startsWith("cam")){
    Serial.println("Camera: ");
    // Todo: camera on/off
  }
  // Move front/back/left/right
  else if (msg.startsWith("go")){
    Serial.println(msg);
  }
  // Watering
  else if (msg.startsWith("watering")){
    Serial.print("Watering"); 
  }
}

void initTemp(){
  dht.setup(SENSOR_TH, DHTesp::DHT11);
  Serial.println("DHT initiated");
}

String getTempHumi(){
  String temHum = "";
  TempAndHumidity newValues = dht.getTempAndHumidity();
  // Check if any reads failed and exit early (to try again).
  if (dht.getStatus() != 0) {
    Serial.println("DHT11 error status: " + String(dht.getStatusString()));
    return "Read error";
  }

  temHum = "Temp:" + String(newValues.temperature) + " Humi:" + String(newValues.humidity);
  Serial.println("Temperature-Humidity: " + temHum);
  return temHum;
}

void initCam() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

  startCameraServer();
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}
  
void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  setupOled();
  setupWifi();
  initTemp();
  initCam();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  long now = millis();
  if (now - lastMsg > 2000) {
    String msg = "Im " + CLIENT_ID;
    client.publish("out", msg.c_str());
    delay(5000);
  }
}
