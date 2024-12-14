#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <VibrationMotor.h>

// ==================== I2C Pin Definitions ====================
#define OLED_SDA 23
#define OLED_SCL 19

// ==================== OLED Display Parameters ====================
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// ==================== OLED Display Initialization ====================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ==================== Vibration Motor Setup ====================
const int motorPin = 18; // Adjust the pin as per your wiring
VibrationMotor myVibrationMotor(motorPin);

// ==================== ESP-NOW Message Structure ====================
typedef struct struct_message {
    char command;          // 'm' for message, 'v' for vibration
    bool Vibrate;          // Indicates if vibration is needed
    char message[100];     // Message to display
} struct_message;

// Instantiate a structure to hold incoming data
struct_message myData;

// ==================== ESP-NOW Callback Function ====================
// Note: Ensure the callback signature matches the ESP32-C6 expectations
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
    if (len < sizeof(struct_message)) {
        Serial.println("Received incomplete data");
        return;
    }

    // Copy the incoming data into the myData structure
    memcpy(&myData, incomingData, sizeof(myData));

    // Handle vibration command
    if (myData.command == 'v') {
        Serial.println("Vibrating motor");
        myVibrationMotor.pulse(2); // Pulse duration in milliseconds
    } 
    // Handle message command
    else if (myData.command == 'm') {
        Serial.print("Received message: ");
        Serial.println(myData.message);

        // Display the message on the OLED
        display.clearDisplay();
        display.setTextSize(1); // Normal 1:1 pixel scale
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0); // Start at top-left corner
        display.println(myData.message);
        display.display();

        // Trigger vibration if requested
        if (myData.Vibrate) {
            Serial.println("Vibrating motor due to message");
            myVibrationMotor.pulse(2); // Pulse duration in milliseconds
        }
    }
}

void setup() {
    // Initialize Serial Communication
    Serial.begin(115200);
    
    Serial.println("ESP32-C6 Receiver Starting...");

    // Initialize I2C with specified pins
    Wire.begin(OLED_SDA, OLED_SCL);

    // Initialize the OLED display
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // 0x3C is a common OLED I2C address
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever
    }

    // Optionally, rotate the display if needed
    // display.setRotation(2); // 0: default, 1: 90deg, 2: 180deg, 3: 270deg

    display.clearDisplay();
    display.display();

    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);
    Serial.println("Wi-Fi Initialized as Station.");

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    Serial.println("ESP-NOW Initialized.");

    // Register the receive callback function
    esp_now_register_recv_cb(OnDataRecv);
    Serial.println("Receive Callback Registered.");

    // Initialize the vibration motor
    // Assuming VibrationMotor library handles initialization in its constructor
    // If not, uncomment the following lines:
    // pinMode(motorPin, OUTPUT);
    // digitalWrite(motorPin, LOW);

    // Display startup message on OLED
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("ESP32-C6 Ready");
    display.display();
    Serial.println("Display Updated: ESP32-C6 Ready");
}

void loop() {
    // Nothing to do here
}