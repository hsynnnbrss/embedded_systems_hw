#include <WiFi.h>
#include "esp_http_server.h"

// ==== WiFi Access Point Configuration ====
const char *ap_ssid = "helloworld";           // WiFi network name (SSID)
const char *ap_password = "helloworld1234";   // WiFi network password

// ==== GPIO Pin Assignments ====
// Motor A (Left Motor) control pins
#define MOTOR_A_IN1 2    // GPIO2 - Motor A direction control 1
#define MOTOR_A_IN2 12   // GPIO12 - Motor A direction control 2

// Motor B (Right Motor) control pins  
#define MOTOR_B_IN1 13   // GPIO13 - Motor B direction control 1
#define MOTOR_B_IN2 15   // GPIO15 - Motor B direction control 2

// HC-SR04 Ultrasonic Distance Sensor pins
#define TRIG_PIN 14      // GPIO14 - Trigger pin (sends ultrasonic pulse)
#define ECHO_PIN 4       // GPIO4 - Echo pin (receives reflected pulse)

// Buzzer/Speaker pin
#define BUZZER_PIN 25    // GPIO25 - Buzzer control pin

// ==== Global Variables ====
httpd_handle_t httpd = NULL;    // HTTP server handle
String command = "";            // Current movement command from web interface

// Function prototype
void handleCommand(const String &cmd);

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(1000);                  // Wait for serial monitor to initialize

  // Configure GPIO pins as inputs/outputs
  pinMode(MOTOR_A_IN1, OUTPUT);  // Set motor control pins as outputs
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_B_IN1, OUTPUT);
  pinMode(MOTOR_B_IN2, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);     // Ultrasonic trigger pin as output
  pinMode(ECHO_PIN, INPUT);      // Ultrasonic echo pin as input
  pinMode(BUZZER_PIN, OUTPUT);   // Buzzer pin as output

  // Initialize all outputs to LOW (motors stopped, buzzer off)
  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN1, LOW);
  digitalWrite(MOTOR_B_IN2, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Configure ESP32 as WiFi Access Point
  WiFi.mode(WIFI_AP);                           // Set WiFi mode to Access Point
  WiFi.softAP(ap_ssid, ap_password);           // Start AP with credentials
  delay(100);                                   // Wait for AP to initialize
  
  // Get and display the Access Point IP address
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // Start the web server for remote control
  startWebServer();

  // Print connection instructions
  Serial.println("Web interface started.");
  Serial.print("Open this address in browser: http://");
  Serial.println(myIP);

  // Startup sound: 3 short beeps to indicate system is ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);   // Turn buzzer on
    delay(100);                       // Beep duration
    digitalWrite(BUZZER_PIN, LOW);    // Turn buzzer off
    delay(100);                       // Pause between beeps
  }
}

void loop() {
  // ==== Obstacle Detection using Ultrasonic Sensor ====
  long duration, distance;
  
  // Send ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);        // Ensure trigger is low
  delayMicroseconds(2);               // Short delay
  digitalWrite(TRIG_PIN, HIGH);       // Send 10µs pulse
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);        // End pulse
  
  // Measure time for echo return (timeout after 30ms)
  duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  // Calculate distance in centimeters
  // Speed of sound = 343 m/s = 0.0343 cm/µs
  // Distance = (time × speed) / 2 (divided by 2 for round trip)
  distance = duration * 0.034 / 2;

  // Check for obstacles within 5cm
  if (distance > 0 && distance <= 5) {
    // Obstacle detected - emergency response
    digitalWrite(BUZZER_PIN, HIGH);     // Sound alarm
    
    // Move backward (reverse both motors)
    digitalWrite(MOTOR_A_IN1, LOW);
    digitalWrite(MOTOR_A_IN2, HIGH);
    digitalWrite(MOTOR_B_IN1, LOW);
    digitalWrite(MOTOR_B_IN2, HIGH);
    
    Serial.println("Obstacle detected! Moving backward...");
  } else {
    // No obstacle - turn off buzzer and execute user commands
    digitalWrite(BUZZER_PIN, LOW);
    handleCommand(command);             // Execute movement command from web
  }

  delay(120);  // Main loop delay (prevents excessive sensor readings)
}

/**
 * Handle movement commands from web interface
 * @param cmd - Command string ("go", "back", "left", "right", "stop")
 */
void handleCommand(const String &cmd) {
  if (cmd == "go") {
    // Move forward - both motors rotate forward
    digitalWrite(MOTOR_A_IN1, HIGH);
    digitalWrite(MOTOR_A_IN2, LOW);
    digitalWrite(MOTOR_B_IN1, HIGH);
    digitalWrite(MOTOR_B_IN2, LOW);
    
  } else if (cmd == "back") {
    // Move backward - both motors rotate backward
    digitalWrite(MOTOR_A_IN1, LOW);
    digitalWrite(MOTOR_A_IN2, HIGH);
    digitalWrite(MOTOR_B_IN1, LOW);
    digitalWrite(MOTOR_B_IN2, HIGH);
    
  } else if (cmd == "left") {
    // Turn left - left motor backward, right motor forward
    digitalWrite(MOTOR_A_IN1, LOW);
    digitalWrite(MOTOR_A_IN2, HIGH);
    digitalWrite(MOTOR_B_IN1, HIGH);
    digitalWrite(MOTOR_B_IN2, LOW);
    
  } else if (cmd == "right") {
    // Turn right - left motor forward, right motor backward
    digitalWrite(MOTOR_A_IN1, HIGH);
    digitalWrite(MOTOR_A_IN2, LOW);
    digitalWrite(MOTOR_B_IN1, LOW);
    digitalWrite(MOTOR_B_IN2, HIGH);
    
  } else if (cmd == "stop") {
    // Stop all motors
    digitalWrite(MOTOR_A_IN1, LOW);
    digitalWrite(MOTOR_A_IN2, LOW);
    digitalWrite(MOTOR_B_IN1, LOW);
    digitalWrite(MOTOR_B_IN2, LOW);
  }
}

// ==== Web Server Implementation ====

/**
 * HTTP request handler for robot movement commands
 * Processes URLs like /go, /back, /left, /right, /stop
 */
static esp_err_t cmd_handler(httpd_req_t *req) {
  // Parse the requested URI and set corresponding command
  if (strstr(req->uri, "/go")) {
    command = "go";
    Serial.println("Command: Forward");
  } else if (strstr(req->uri, "/back")) {
    command = "back";
    Serial.println("Command: Backward");
  } else if (strstr(req->uri, "/left")) {
    command = "left";
    Serial.println("Command: Left");
  } else if (strstr(req->uri, "/right")) {
    command = "right";
    Serial.println("Command: Right");
  } else if (strstr(req->uri, "/stop")) {
    command = "stop";
    Serial.println("Command: Stop");
  } else {
    // Unknown command - return 404 error
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  
  // Set response headers for CORS and content type
  httpd_resp_set_hdr(req, "Content-Type", "text/plain; charset=utf-8");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  
  // Send success response
  return httpd_resp_send(req, "OK", 2);
}

/**
 * HTTP request handler for the main web page
 * Serves the HTML control interface
 */
static esp_err_t index_handler(httpd_req_t *req) {
  // HTML page with responsive robot control interface
  const char *html =
    "<!DOCTYPE html>"
    "<html lang=\"en\">"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>ESP32 Robot Control</title>"
    "<style>"
    // CSS styling for mobile-friendly interface
    "body{font-family:Arial,sans-serif;text-align:center;margin:10px;padding:0;background:#f0f0f0}"
    ".container{max-width:600px;margin:0 auto;background:white;padding:15px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
    "h1{color:#333;margin-top:0}"
    ".button{background:#4CAF50;color:white;border:none;padding:12px;width:80%;max-width:200px;margin:5px auto;display:block;font-size:18px;border-radius:5px;cursor:pointer;transition:background 0.3s}"
    ".button:hover{background:#45a049}"
    ".button:active{background:#357a38}"
    ".controls{margin:10px 0}"
    ".row{display:flex;justify-content:space-between;max-width:300px;margin:0 auto}"
    ".col{width:32%}"
    ".status{font-size:14px;color:#666;margin-top:15px}"
    "</style>"
    "</head>"
    "<body>"
    "<div class=\"container\">"
    "<h1>ESP32 Robot</h1>"
    "<div class=\"controls\">"
    // Control buttons layout: Forward, Left-Stop-Right, Backward
    "<button class=\"button\" onclick=\"sendCommand('go')\">FORWARD</button>"
    "<div class=\"row\">"
    "<div class=\"col\"><button class=\"button\" style=\"width:100%\" onclick=\"sendCommand('left')\">LEFT</button></div>"
    "<div class=\"col\"><button class=\"button\" style=\"width:100%\" onclick=\"sendCommand('stop')\">STOP</button></div>"
    "<div class=\"col\"><button class=\"button\" style=\"width:100%\" onclick=\"sendCommand('right')\">RIGHT</button></div>"
    "</div>"
    "<button class=\"button\" onclick=\"sendCommand('back')\">BACK</button>"
    "<p class=\"status\">Status: <span id=\"status\">Ready</span></p>"
    "</div>"
    "</div>"
    "<script>"
    // JavaScript function to send commands to ESP32
    "function sendCommand(cmd) {"
    "  document.getElementById('status').innerText = cmd + ' command sent';"  // Update status display
    "  fetch('/' + cmd)"                                                      // Send HTTP request
    "    .then(response => console.log('Command sent: ' + cmd))"             // Log success
    "    .catch(error => console.error('Error: ', error));"                  // Log errors
    "}"
    "</script>"
    "</body>"
    "</html>";

  // Send the HTML page as response
  httpd_resp_sendstr_chunk(req, html);
  httpd_resp_sendstr_chunk(req, NULL);  // End of response
  return ESP_OK;
}

/**
 * Initialize and configure the web server
 * Sets up HTTP routes and starts the server on port 80
 */
void startWebServer() {
  // Configure HTTP server settings
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;        // Standard HTTP port
  config.stack_size = 8192;       // Stack size for server tasks

  // Define route for main page (/)
  httpd_uri_t index_uri = {
    .uri = "/",                   // Root URL
    .method = HTTP_GET,           // GET method
    .handler = index_handler,     // Handler function
    .user_ctx = NULL
  };

  // Define route for all command URLs (/* wildcard)
  httpd_uri_t cmd_uri = {
    .uri = "/*",                  // Match all URLs
    .method = HTTP_GET,           // GET method
    .handler = cmd_handler,       // Handler function
    .user_ctx = NULL
  };

  // Start the HTTP server
  Serial.printf("Starting web server - port: %d\n", config.server_port);
  if (httpd_start(&httpd, &config) == ESP_OK) {
    // Register the URL handlers
    httpd_register_uri_handler(httpd, &index_uri);  // Main page handler
    httpd_register_uri_handler(httpd, &cmd_uri);    // Command handler
    Serial.println("Web server started successfully");
  } else {
    Serial.println("Failed to start web server");
  }
}
