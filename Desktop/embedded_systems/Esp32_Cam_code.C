#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ==== WiFi Access Point Configuration ====
const char *ap_ssid = "helloworld";           // WiFi network name (SSID)
const char *ap_password = "helloworld1234";   // WiFi network password

// ==== GPIO Pin Assignments for Motors and Sensors ====
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
#define BUZZER_PIN 16    // GPIO16 - Buzzer control pin

// ==== ESP32-CAM Module Pin Configuration ====
// Camera module GPIO pin assignments for ESP32-CAM board
#define PWDN_GPIO_NUM     32   // Power down pin (camera enable/disable)
#define RESET_GPIO_NUM    -1   // Reset pin (not used, set to -1)
#define XCLK_GPIO_NUM     0    // External clock pin
#define SIOD_GPIO_NUM     26   // I2C data pin for camera configuration
#define SIOC_GPIO_NUM     27   // I2C clock pin for camera configuration
#define Y9_GPIO_NUM       35   // Camera data pin 9 (MSB)
#define Y8_GPIO_NUM       34   // Camera data pin 8
#define Y7_GPIO_NUM       39   // Camera data pin 7
#define Y6_GPIO_NUM       36   // Camera data pin 6
#define Y5_GPIO_NUM       21   // Camera data pin 5
#define Y4_GPIO_NUM       19   // Camera data pin 4
#define Y3_GPIO_NUM       18   // Camera data pin 3
#define Y2_GPIO_NUM       5    // Camera data pin 2 (LSB)
#define VSYNC_GPIO_NUM    25   // Vertical sync pin
#define HREF_GPIO_NUM     23   // Horizontal reference pin
#define PCLK_GPIO_NUM     22   // Pixel clock pin

// ==== MJPEG Video Stream Constants ====
// HTTP headers and boundaries for MJPEG streaming protocol
const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=123456789000000000000987654321";
const char *_STREAM_BOUNDARY = "\r\n--123456789000000000000987654321\r\n";
const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ==== Global Variables ====
httpd_handle_t camera_httpd = NULL;    // HTTP server handle for camera and control
String command = "";                   // Current movement command from web interface
bool camera_initialized = false;      // Flag to track camera initialization status
bool client_connected = false;        // Flag to track if client is viewing stream

// Function prototypes
void handleCommand(const String &cmd);
bool initCamera();
static esp_err_t stream_handler(httpd_req_t *req);
static esp_err_t index_handler(httpd_req_t *req);
static esp_err_t cmd_handler(httpd_req_t *req);
void startCameraServer();

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(1000);                        // Wait for serial monitor to initialize

  // Configure GPIO pins for motors, sensors, and buzzer
  pinMode(MOTOR_A_IN1, OUTPUT);       // Set motor control pins as outputs
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_B_IN1, OUTPUT);
  pinMode(MOTOR_B_IN2, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);          // Ultrasonic trigger pin as output
  pinMode(ECHO_PIN, INPUT);           // Ultrasonic echo pin as input
  pinMode(BUZZER_PIN, OUTPUT);        // Buzzer pin as output

  // Initialize all outputs to LOW (motors stopped, buzzer off)
  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN1, LOW);
  digitalWrite(MOTOR_B_IN2, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize camera module
  camera_initialized = initCamera();
  if (camera_initialized) {
    Serial.println("Camera initialization successful");
  } else {
    Serial.println("Camera initialization failed - robot will work without camera");
  }

  // Configure ESP32 as WiFi Access Point only
  WiFi.mode(WIFI_AP);                           // Set WiFi mode to Access Point
  WiFi.softAP(ap_ssid, ap_password);           // Start AP with credentials
  delay(100);                                   // Wait for AP to initialize
  
  // Get and display the Access Point IP address
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // Start the web server for camera streaming and robot control
  startCameraServer();

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
  long sure, mesafe;  // duration and distance variables
  
  // Send ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);        // Ensure trigger is low
  delayMicroseconds(2);               // Short delay
  digitalWrite(TRIG_PIN, HIGH);       // Send 10µs pulse
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);        // End pulse
  
  // Measure time for echo return (timeout after 30ms)
  sure = pulseIn(ECHO_PIN, HIGH, 30000);
  
  // Calculate distance in centimeters
  // Speed of sound = 343 m/s = 0.0343 cm/µs
  // Distance = (time × speed) / 2 (divided by 2 for round trip)
  mesafe = sure * 0.034 / 2;

  // Check for obstacles within 5cm
  if (mesafe > 0 && mesafe <= 5) {
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

/**
 * Initialize ESP32-CAM camera module
 * @return true if initialization successful, false otherwise
 */
bool initCamera() {
  camera_config_t config;
  
  // Configure camera hardware settings
  config.ledc_channel = LEDC_CHANNEL_0;     // LED PWM channel for camera clock
  config.ledc_timer = LEDC_TIMER_0;         // LED PWM timer for camera clock
  
  // Assign camera data pins (8-bit parallel interface)
  config.pin_d0 = Y2_GPIO_NUM;              // Data bit 0 (LSB)
  config.pin_d1 = Y3_GPIO_NUM;              // Data bit 1
  config.pin_d2 = Y4_GPIO_NUM;              // Data bit 2
  config.pin_d3 = Y5_GPIO_NUM;              // Data bit 3
  config.pin_d4 = Y6_GPIO_NUM;              // Data bit 4
  config.pin_d5 = Y7_GPIO_NUM;              // Data bit 5
  config.pin_d6 = Y8_GPIO_NUM;              // Data bit 6
  config.pin_d7 = Y9_GPIO_NUM;              // Data bit 7 (MSB)
  
  // Assign camera control pins
  config.pin_xclk = XCLK_GPIO_NUM;          // External clock pin
  config.pin_pclk = PCLK_GPIO_NUM;          // Pixel clock pin
  config.pin_vsync = VSYNC_GPIO_NUM;        // Vertical sync pin
  config.pin_href = HREF_GPIO_NUM;          // Horizontal reference pin
  config.pin_sccb_sda = SIOD_GPIO_NUM;      // I2C data pin (SCCB protocol)
  config.pin_sccb_scl = SIOC_GPIO_NUM;      // I2C clock pin (SCCB protocol)
  config.pin_pwdn = PWDN_GPIO_NUM;          // Power down control pin
  config.pin_reset = RESET_GPIO_NUM;        // Reset pin (not used)
  
  // Camera timing and quality settings
  config.xclk_freq_hz = 20000000;           // 20MHz external clock frequency
  config.pixel_format = PIXFORMAT_JPEG;     // Output format: JPEG compression
  config.frame_size = FRAMESIZE_QQVGA;      // Resolution: 160x120 pixels (small for streaming)
  config.jpeg_quality = 12;                 // JPEG quality (0-63, lower = better quality)
  config.fb_count = 1;                      // Number of frame buffers (1 for streaming)
  config.fb_location = CAMERA_FB_IN_DRAM;   // Frame buffer location in DRAM

  // Initialize camera with configuration
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera initialization failed: 0x%x\n", err);
    return false;
  }
  Serial.println("Camera initialized successfully!");
  return true;
}

/**
 * Initialize and configure the web server
 * Sets up HTTP routes for control interface, camera stream, and commands
 */
void startCameraServer() {
  // Configure HTTP server settings
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;        // Standard HTTP port
  config.stack_size = 8192;       // Stack size for server tasks

  // Define route for main control page (/)
  httpd_uri_t index_uri = {
    .uri = "/",                   // Root URL
    .method = HTTP_GET,           // GET method
    .handler = index_handler,     // Handler function
    .user_ctx = NULL
  };

  // Define route for all command URLs (/* wildcard)
  httpd_uri_t cmd_uri = {
    .uri = "/*",                  // Match all URLs (except specific ones)
    .method = HTTP_GET,           // GET method
    .handler = cmd_handler,       // Handler function
    .user_ctx = NULL
  };

  // Define route for camera video stream (/stream)
  httpd_uri_t stream_uri = {
    .uri = "/stream",             // Stream endpoint URL
    .method = HTTP_GET,           // GET method
    .handler = stream_handler,    // Handler function for MJPEG stream
    .user_ctx = NULL
  };

  // Start the HTTP server
  Serial.printf("Starting web server - port: %d\n", config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    // Register the URL handlers in order (most specific first)
    httpd_register_uri_handler(camera_httpd, &index_uri);    // Main page handler
    httpd_register_uri_handler(camera_httpd, &stream_uri);   // Camera stream handler
    httpd_register_uri_handler(camera_httpd, &cmd_uri);      // Command handler (catch-all)
    Serial.println("Web server started successfully");
  } else {
    Serial.println("Failed to start web server");
  }
}

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
 * HTTP request handler for MJPEG video streaming
 * Continuously streams camera frames as MJPEG format
 */
static esp_err_t stream_handler(httpd_req_t *req) {
  Serial.println("Video stream started");
  
  // Set HTTP headers for MJPEG streaming
  httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  
  client_connected = true;              // Mark client as connected
  uint8_t *_jpg_buf = NULL;            // JPEG image buffer pointer
  size_t _jpg_buf_len = 0;             // JPEG image buffer length
  int64_t start_time = esp_timer_get_time();  // Start time for FPS calculation
  int frame_count = 0;                 // Frame counter for FPS calculation

  // Main streaming loop - continues until client disconnects
  while (client_connected) {
    // Capture a frame from camera
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera frame capture failed");
      client_connected = false;
      break;
    }
    
    // Get JPEG data from frame buffer
    _jpg_buf_len = fb->len;            // JPEG data length
    _jpg_buf = fb->buf;                // JPEG data buffer
    
    // Prepare MJPEG frame header
    char part_buf[64];
    size_t hlen = snprintf(part_buf, 64, _STREAM_PART, _jpg_buf_len);

    // Send MJPEG boundary and header
    if (httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY)) != ESP_OK ||
        httpd_resp_send_chunk(req, part_buf, hlen) != ESP_OK) {
      client_connected = false;
    }

    // Send JPEG image data in chunks (to handle large images)
    if (client_connected) {
      const size_t chunk_size = 1024;  // Send 1KB chunks at a time
      for (size_t i = 0; i < _jpg_buf_len && client_connected; i += chunk_size) {
        size_t chunk = _jpg_buf_len - i < chunk_size ? _jpg_buf_len - i : chunk_size;
        if (httpd_resp_send_chunk(req, (const char *)&_jpg_buf[i], chunk) != ESP_OK) {
          client_connected = false;
          break;
        }
        yield();  // Allow other tasks to run during transmission
      }
    }

    // Release the camera frame buffer
    esp_camera_fb_return(fb);
    fb = NULL;
    _jpg_buf = NULL;

    // Calculate and display FPS every 10 frames
    frame_count++;
    if (frame_count % 10 == 0) {
      int64_t current_time = esp_timer_get_time();
      float seconds = (current_time - start_time) / 1000000.0;
      float fps = frame_count / seconds;
      Serial.printf("Streaming at %.1f fps\n", fps);
    }
    
    delay(100);  // Control frame rate (approximately 10 FPS)
  }
  
  Serial.println("Video stream ended");
  client_connected = false;
  return ESP_OK;
}

/**
 * HTTP request handler for the main web page
 * Serves the HTML control interface with camera stream
 */
static esp_err_t index_handler(httpd_req_t *req) {
  // HTML page header with responsive design
  const char *html =
    "<!DOCTYPE html>"
    "<html lang=\"en\">"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>ESP32-CAM Robot Control</title>"
    "<style>"
    // CSS styling for mobile-friendly interface
    "body{font-family:Arial,sans-serif;text-align:center;margin:10px;padding:0;background:#f0f0f0}"
    ".container{max-width:600px;margin:0 auto;background:white;padding:15px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
    "h1{color:#333;margin-top:0}"
    ".stream-container{position:relative;margin:10px 0;overflow:hidden;border-radius:5px;border:1px solid #ccc}"
    ".stream{display:block;width:100%;height:auto;margin:0 auto}"
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
    "<h1>ESP32-CAM Robot</h1>";

  // Send HTML header
  httpd_resp_sendstr_chunk(req, html);

  // Add camera stream section (if camera is available)
  if (camera_initialized) {
    const char *html_with_camera =
      "<div class=\"stream-container\" style=\"width:180px;height:140px;display:flex;align-items:center;justify-content:center;\">"
      "<img src=\"/stream\" class=\"stream\" alt=\"Camera feed\" "
      "style=\"width:160px;height:120px;object-fit:contain;box-shadow:0 0 8px #999;border-radius:4px;\">"
      "</div>";
    httpd_resp_sendstr_chunk(req, html_with_camera);
  } else {
    // Show error message if camera is not available
    const char *html_no_camera =
      "<div class=\"stream-container\" style=\"height:60px;display:flex;align-items:center;justify-content:center\">"
      "<p style=\"color:red\">Camera not available</p>"
      "</div>";
    httpd_resp_sendstr_chunk(req, html_no_camera);
  }

  // Add control buttons and JavaScript
  const char *html_controls =
    "<div class=\"controls\">"
    // Control buttons layout: Forward, Left-Stop-Right, Backward
    "<button class=\"button\" onclick=\"sendCommand('go')\">FORWARD</button>"
    "<div class=\"row\">"
    "<div class=\"col\"><button class=\"button\" style=\"width:100%\" onclick=\"sendCommand('left')\">LEFT</button></div>"
    "<div class=\"col\"><button class=\"button\" style=\"width:100%\" onclick=\"sendCommand('stop')\">STOP</button></div>"
    "<div class=\"col\"><button class=\"button\" style=\"width:100%\" onclick=\"sendCommand('right')\">RIGHT</button></div>"
    "</div>"
    "<button class=\"button\" onclick=\"sendCommand('back')\">BACKWARD</button>"
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

  // Send control interface and end response
  httpd_resp_sendstr_chunk(req, html_controls);
  httpd_resp_sendstr_chunk(req, NULL);  // End of response

  return ESP_OK;
}