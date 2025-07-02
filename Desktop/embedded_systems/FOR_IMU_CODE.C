#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <MPU6050.h>
#include <LiquidCrystal_I2C.h>

// WiFi credentials
const char* ssid = "helloworld";
const char* password = "helloworld1234";

// ESP32-CAM IP address (Access Point)
const char* cam_host = "http://192.168.4.1";  // ESP32-CAM AP IP

MPU6050 imu;

// LCD I2C address and size
LiquidCrystal_I2C lcd(0x27, 16, 2); // 16x2 LCD

String lastCommand = "";

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // Initialize I2C on pins SDA=21, SCL=22

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("IMU LCD Kontrol");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.println(WiFi.localIP());

  lcd.setCursor(0, 1);
  lcd.print("WiFi connected");

  // Initialize the IMU (MPU6050)
  imu.initialize();
  if (!imu.testConnection()) {
    Serial.println("MPU6050 connection error!");
    lcd.setCursor(0, 1);
    lcd.print("IMU ERROR!      ");
    while (1); // Halt if IMU not found
  } else {
    Serial.println("MPU6050 connected.");
    lcd.setCursor(0, 1);
    lcd.print("IMU connected   ");
  }

  delay(1500); // Wait for user to see the status
  lcd.clear(); // Clear LCD for main loop display
}

void loop() {
  int16_t ax, ay, az; // Variables for acceleration data
  imu.getAcceleration(&ax, &ay, &az); // Read acceleration

  String command = "";

  // Determine movement direction based on acceleration readings
  if (ay < -10000) {
    command = "go";
  } else if (ay > 10000) {
    command = "back";
  } else if (ax > 8000) {
    command = "right";
  } else if (ax < -8000) {
    command = "left";
  } else {
    command = "stop";
  }

  // Display direction on LCD
  lcd.setCursor(0, 0);
  lcd.print("Direction:            "); // Clear the row
  lcd.setCursor(5, 0);              // Start after "Direction: "
  lcd.print(command);

  // Display acceleration values on LCD
  lcd.setCursor(0, 1);
  lcd.print("AX:");
  lcd.print(ax);
  lcd.print(" AY:");
  lcd.print(ay);

  // Only send command if it changed since last loop
  if (command != lastCommand) {
    sendCommand(command);
    lastCommand = command;
  }

  delay(300); // Loop delay
}

// Send movement command to ESP32-CAM via HTTP GET request
void sendCommand(const String& cmd) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(cam_host) + "/" + cmd;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Command sent: " + cmd + " (" + String(httpCode) + ")");
    } else {
      Serial.println("HTTP error: " + http.errorToString(httpCode));
    }
    http.end();
  } else {
    Serial.println("No WiFi connection.");
  }
}
