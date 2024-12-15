#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#define OLED_SDA 23
#define OLED_SCL 19
#define MOTOR_PIN 18
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

// Message structure for ESP-NOW
typedef struct struct_message {
    char command;
    bool Vibrate;
    char message[100];
} struct_message;

struct_message myData;

// Function to update display
void updateDisplay(const char* msg) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0,20);
    display.println(msg);
    display.display();
}

// Function to trigger vibration
void vibrate(int duration) {
    digitalWrite(MOTOR_PIN, HIGH);
    delay(duration);
    digitalWrite(MOTOR_PIN, LOW);
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("BLE Connected");
        updateDisplay("Connected");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("BLE Disconnected");
        updateDisplay("Waiting..");
        BLEDevice::startAdvertising();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        uint8_t* data = pCharacteristic->getData();
        size_t len = pCharacteristic->getLength();

        if (len > 0) {
            char msgBuffer[100] = {0};
            size_t copyLen = std::min(len, sizeof(msgBuffer) - 1);
            memcpy(msgBuffer, data, copyLen);
            
            Serial.printf("BLE Received: %s\n", msgBuffer);
            updateDisplay(msgBuffer);
            vibrate(100);
        }
    }
};

void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
    if (len == sizeof(struct_message)) {
        memcpy(&myData, incomingData, sizeof(myData));
        
        if (myData.command == 'v') {
            Serial.println("ESP-NOW: Vibrate command");
            vibrate(200);
        }
        else if (myData.command == 'm') {
            Serial.printf("ESP-NOW Message: %s\n", myData.message);
            updateDisplay(myData.message);
            if (myData.Vibrate) vibrate(100);
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting...");

    // Initialize Motor
    pinMode(MOTOR_PIN, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);

    // Initialize I2C and OLED
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 failed");
        return;
    }
    display.setTextColor(SSD1306_WHITE);

    // Initialize WiFi for ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW failed");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);

    // Initialize BLE
    BLEDevice::init("ESP32-C6");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ |
                        BLECharacteristic::PROPERTY_WRITE
                      );

    pCharacteristic->setCallbacks(new MyCallbacks());
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    updateDisplay("Ready!");
    Serial.println("Setup Complete!");
}

void loop() {
    delay(100);
}