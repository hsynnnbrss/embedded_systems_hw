# Embedded Systems Project

This project contains code for an ESP32-based robotic system with camera streaming, motor control, obstacle detection, and IMU-based remote control. The system is designed for educational and prototyping purposes, demonstrating how to integrate various sensors and modules with the ESP32 platform.

## Project Structure

- **Esp32_Cam_code.C**: Implements a robot controller using the ESP32-CAM module. It provides:
  - WiFi Access Point mode for direct connection.
  - A web interface for controlling the robot's movement (forward, backward, left, right, stop).
  - Real-time MJPEG video streaming from the onboard camera.
  - Obstacle detection using an ultrasonic sensor, with automatic emergency stop and buzzer alarm.
  - Motor and buzzer control via GPIO pins.

- **Motor_Esp_32_Code.C**: Similar to the above, but for a standard ESP32 (without camera). It provides:
  - WiFi Access Point mode and a web interface for remote motor control.
  - Obstacle detection and buzzer alarm.
  - Motor control for a two-wheel robot.

- **FOR_IMU_CODE.C**: Implements an IMU-based remote controller using another ESP32 board with an MPU6050 sensor and LCD display. It provides:
  - WiFi client mode to connect to the ESP32-CAM's access point.
  - Reads acceleration data from the IMU to determine movement commands (go, back, left, right, stop).
  - Sends movement commands to the ESP32-CAM robot via HTTP requests.
  - Displays direction and acceleration data on an I2C LCD.

## How It Works

1. **Robot (ESP32/ESP32-CAM):**
   - Set up as a WiFi Access Point. Users can connect directly to its network.
   - Provides a web interface for manual control and, if using ESP32-CAM, live video streaming.
   - Uses an ultrasonic sensor to detect obstacles and automatically stops or reverses if something is too close.
   - Motors and buzzer are controlled via GPIO pins.

2. **IMU Remote (ESP32 + MPU6050):**
   - Connects to the robot's WiFi network.
   - Reads IMU data to determine the intended movement direction.
   - Sends HTTP commands to the robot to control its movement.
   - Displays status and sensor data on an LCD.

## Requirements

- ESP32 or ESP32-CAM development boards
- HC-SR04 Ultrasonic Distance Sensor
- L298N or similar motor driver (for controlling motors)
- MPU6050 IMU sensor (for remote controller)
- I2C LCD display (for remote controller)
- Buzzer
- Basic electronic components and wiring

## Usage

1. **Upload the appropriate code to each ESP32 board:**
   - `Esp32_Cam_code.C` for the ESP32-CAM robot
   - `Motor_Esp_32_Code.C` for a standard ESP32 robot (no camera)
   - `FOR_IMU_CODE.C` for the IMU-based remote controller

2. **Power on the robot.** It will create a WiFi network (SSID: `helloworld`, Password: `helloworld1234`).

3. **Connect to the robot's WiFi network** from your phone or computer. Open a browser and go to `http://192.168.4.1` to access the control interface (and video stream if using ESP32-CAM).

4. **Power on the IMU remote controller.** It will connect to the robot's WiFi and send movement commands based on its orientation.

## Notes

- Make sure to install the required libraries for ESP32, ESP32-CAM, MPU6050, and LCD in your Arduino IDE or PlatformIO environment.
- Pin assignments may need to be adjusted based on your hardware setup.
- The system is intended for educational and prototyping use.

## License

This project is open source and free to use for educational purposes. 