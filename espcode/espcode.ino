#include <Arduino.h>
#include <Ticker.h>
#include "EMGFilters.h"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <esp_now.h>
#include <WiFi.h>

// ==================== PWMBOARD Definitions ====================
Adafruit_PWMServoDriver pwmDriver = Adafruit_PWMServoDriver(0x40);

// Servo channel constants
const int NUM_SERVOS = 3;

// Struct to hold servo configurations
struct MotorConfig {
    int channel;          // PWM channel number
    int initialAngle;     // Initial angle in degrees
    int activeAngle;      // Active angle in degrees
};

// Array of motor configurations
MotorConfig motors[NUM_SERVOS] = {
    {0, 180, 50},   // Motor 0: initial 180°, active 50°
    {1, 0, 170},    // Motor 1: initial 0°, active 130°
    {2, 0, 90}      // Motor 2: initial 0°, active 80°
};

// Pulse width boundaries
#define SERVOMIN  125
#define SERVOMAX  625

// ==================== Pin Definitions ====================
const int SensorInputPin = 33; // ADC1_CH6
const int ledPin = 2;          // Onboard LED

// ==================== EMG Filter Configuration ====================
EMGFilters myFilter;
SAMPLE_FREQUENCY sampleRate = SAMPLE_FREQ_1000HZ;
NOTCH_FREQUENCY humFreq = NOTCH_FREQ_50HZ;

// ==================== Calibration Parameters ====================
const unsigned long calibrationTime = 5000; // Total calibration time (5 seconds)
float restLevel = 0.0;
float activeLevel = 0.0;

// ==================== Threshold Parameters ====================
float UpperThreshold = 0.0;
float LowerThreshold = 0.0;

// ==================== Signal Smoothing Parameters ====================
const int smoothingWindowSize = 50;
float smoothingBuffer[smoothingWindowSize] = {0};
int smoothingIndex = 0;
float smoothingSum = 0.0;
bool bufferFilled = false;

// ==================== State Machine States ====================
enum EMGState { IDLE, ACTIVE };
EMGState currentState = IDLE;

// ==================== Debounce Parameters ====================
const unsigned long stateChangeDelay = 300; // 300ms debounce
unsigned long lastStateChangeTime = 0;

// ==================== Volatile Variables ====================
volatile int Value = 0;
volatile bool newData = false;

// ==================== Ticker Setup ====================
Ticker timerTicker;

// ==================== ESP-NOW Definitions ====================

// Replace with your ESP32-C6 Xiao's MAC Address
// You can find it using Serial.println(WiFi.macAddress()) on the receiver board
uint8_t receiverMacAddress[] = {0x54, 0x32, 0x04, 0x21, 0x7A, 0xFC}; // Example MAC Address

// Structure to send data
typedef struct struct_message {
    char command;          // 'm' for message, 'v' for vibration
    bool Vibrate;          // Indicates if vibration is needed
    char message[100];     // Message to display
} struct_message;

struct_message myData;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Function to send messages
void sendMessage(bool Vibrate, const char* message) {
    myData.command = 'm'; // 'm' for message
    myData.Vibrate = Vibrate;
    strncpy(myData.message, message, sizeof(myData.message) - 1);
    myData.message[sizeof(myData.message) - 1] = '\0'; // Ensure null-termination

    esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&myData, sizeof(myData));
    if (result == ESP_OK) {
        Serial.println("Sent message with success");
    } else {
        Serial.println("Error sending the message");
    }

    // Optionally trigger vibration
    if (Vibrate) {
        sendVibrationCommand();
    }
}

// Function to send vibration command
void sendVibrationCommand() {
    myData.command = 'v'; // 'v' for vibration
    myData.Vibrate = true;
    myData.message[0] = '\0'; // No message needed

    esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&myData, sizeof(myData));
    if (result == ESP_OK) {
        Serial.println("Sent vibration command successfully");
    } else {
        Serial.println("Error sending the vibration command");
    }
}

// ==================== ISR for Timer ====================
void onTick() {
    newData = true;
}

void setup() {
    // Initialize Serial Communication
    Serial.begin(115200);
    while (!Serial); // Wait for Serial to initialize

    // Initialize I2C Communication
    Wire.begin(21, 22); // SDA=21, SCL=22
    Serial.println("Full arm testing");

    // Initialize PWM Driver
    pwmDriver.begin();
    pwmDriver.setPWMFreq(60); // Set frequency to 60 Hz for servos
    Serial.println("PWM Servo Driver Initialized.");

    // Initialize all Servos to their initial positions
    setAllServoPositions(false); // false indicates setting to initial positions

    // Initialize LED Pin
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW); // Ensure LED is off initially

    // Initialize EMG Filter
    myFilter.init(sampleRate, humFreq, true, true, true);
    Serial.println("EMG Filter Initialized.");

    // Set ADC resolution and attenuation
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db); // Adjust as needed

    // Initialize Ticker for 1ms interval
    timerTicker.attach_ms(1, onTick); // 1ms interval

    // ==================== ESP-NOW Initialization ====================
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Register send callback
    esp_now_register_send_cb(OnDataSent);

    // Register peer
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, receiverMacAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add peer");
        return;
    }

    // ==================== Calibration ====================
    Serial.println("Starting calibration...");
    sendMessage(false, "Starting calibration...");

    calibrateEMG();

    Serial.println("Calibration complete.");
    sendMessage(false, "Calibration complete.");

    Serial.print("Rest Level: ");
    Serial.println(restLevel);
    String restMsg = "Rest Level: " + String(restLevel, 2);
    sendMessage(false, restMsg.c_str());

    Serial.print("Active Level: ");
    Serial.println(activeLevel);
    String activeMsg = "Active Level: " + String(activeLevel, 2);
    sendMessage(false, activeMsg.c_str());

    // Set thresholds based on calibration
    UpperThreshold = (restLevel + activeLevel) / 2 + (activeLevel - restLevel) * 0.2;
    LowerThreshold = (restLevel + activeLevel) / 2 - (activeLevel - restLevel) * 0.2;

    Serial.print("UpperThreshold: ");
    Serial.println(UpperThreshold);
    String upperMsg = "Upper Threshold: " + String(UpperThreshold, 2);
    sendMessage(false, upperMsg.c_str());

    Serial.print("LowerThreshold: ");
    Serial.println(LowerThreshold);
    String lowerMsg = "Lower Threshold: " + String(LowerThreshold, 2);
    sendMessage(false, lowerMsg.c_str());
}

void loop() {
    if (newData) {
        newData = false;

        // Read the sensor value
        Value = analogRead(SensorInputPin);

        // Filter processing
        float DataAfterFilter = myFilter.update(Value);

        // Envelope detection
        float envelope = abs(DataAfterFilter);

        // Smoothing using moving average
        smoothingSum -= smoothingBuffer[smoothingIndex];
        smoothingBuffer[smoothingIndex] = envelope;
        smoothingSum += smoothingBuffer[smoothingIndex];
        smoothingIndex = (smoothingIndex + 1) % smoothingWindowSize;
        if (smoothingIndex == 0) bufferFilled = true;

        float smoothedEnvelope = smoothingSum / (bufferFilled ? smoothingWindowSize : smoothingIndex);

        // Send smoothed data to Serial Monitor (for graphing)
        Serial.println(smoothedEnvelope);

        // State machine logic with debouncing
        unsigned long currentTime = millis();

        switch (currentState) {
            case IDLE:
                if (smoothedEnvelope > UpperThreshold) {
                    if (currentTime - lastStateChangeTime > stateChangeDelay) {
                        currentState = ACTIVE;
                        digitalWrite(ledPin, HIGH); // Turn LED on
                        setAllServoPositions(true);  // true indicates setting to active positions
                        lastStateChangeTime = currentTime;
                        Serial.println("State changed to ACTIVE");
                        sendMessage(true, "State changed to ACTIVE"); // Sends vibration and message
                    }
                }
                break;

            case ACTIVE:
                if (smoothedEnvelope < LowerThreshold) {
                    if (currentTime - lastStateChangeTime > stateChangeDelay) {
                        currentState = IDLE;
                        digitalWrite(ledPin, LOW);  // Turn LED off
                        setAllServoPositions(false); // false indicates setting to initial positions
                        lastStateChangeTime = currentTime;
                        Serial.println("State changed to IDLE");
                        sendMessage(false, "State changed to IDLE"); // Sends message without vibration
                    }
                }
                break;
        }
    }
}

void calibrateEMG() {
    unsigned long startTime = millis();
    float restSum = 0.0;
    float activeSum = 0.0;
    int restCount = 0;
    int activeCount = 0;

    // Calibration Phase 1: Rest
    Serial.println("Please keep your hand relaxed for calibration...");
    sendMessage(false, "Relax hand for calibration");

    while (millis() - startTime < calibrationTime / 2) {
        if (newData) {
            newData = false;
            int sensorValue = analogRead(SensorInputPin);
            float filtered = myFilter.update(sensorValue);
            float env = abs(filtered);
            restSum += env;
            restCount++;
        }
    }

    restLevel = (restCount > 0) ? (restSum / restCount) : 0;

    // Calibration Phase 2: Active
    Serial.println("Now, please perform the action (e.g., close your hand) for calibration...");
    sendVibrationCommand();
    sendMessage(false, "Perform action for calibration");

    unsigned long activeStartTime = millis();
    while (millis() - activeStartTime < calibrationTime / 2) {
        if (newData) {
            newData = false;
            int sensorValue = analogRead(SensorInputPin);
            float filtered = myFilter.update(sensorValue);
            float env = abs(filtered);
            activeSum += env;
            activeCount++;
        }
    }

    activeLevel = (activeCount > 0) ? (activeSum / activeCount) : 0;
}

void setAllServoPositions(bool active) {
    for (int i = 0; i < NUM_SERVOS; i++) {
        int angle = active ? motors[i].activeAngle : motors[i].initialAngle;
        int pulse = angleToPulse(angle);
        pwmDriver.setPWM(motors[i].channel, 0, pulse);
    }
}

// Function to map angle to PWM pulse length
int angleToPulse(int ang) {
    // Ensure angle is within 0-180 degrees
    ang = constrain(ang, 0, 180);
    return map(ang, 0, 180, SERVOMIN, SERVOMAX);
}