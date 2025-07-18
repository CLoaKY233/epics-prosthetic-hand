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

uint8_t receiverMacAddress[] = {0x24 ,0x6F ,0x28 ,0x16 ,0x76 ,0x30};

typedef struct struct_message {
    char command;
    bool Vibrate;
    char message[100];
    int motorNumber;
    int angle;
} struct_message;

struct_message myData;

void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
    struct_message* msg = (struct_message*)incomingData;

    Serial.println("Received ESP-NOW message");

    if (msg->command == 'm') {
        updateDisplay(msg->message);
        if (msg->Vibrate) {
            vibrate(100);
        }
    }
    else if (msg->command == 'v') {
        vibrate(100);
    }
}

void updateDisplay(const char* msg) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0,20);
    display.println(msg);
    display.display();
}

void vibrate(int duration) {
    digitalWrite(MOTOR_PIN, HIGH);
    delay(duration);
    digitalWrite(MOTOR_PIN, LOW);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
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

            if (msgBuffer[0] == 'M' && msgBuffer[1] >= '1' && msgBuffer[1] <= '3') {
                int motorNum = msgBuffer[1] - '0';
                int angle = atoi(&msgBuffer[3]);

                struct_message msg;
                msg.command = 'o';
                msg.motorNumber = motorNum;
                msg.angle = angle;
                strcpy(msg.message, msgBuffer);

                esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&msg, sizeof(msg));
                if (result == ESP_OK) {
                    Serial.println("Sent override command successfully");
                }
            }
            else if (strcmp(msgBuffer, "STOP") == 0) {
                struct_message msg;
                msg.command = 's';
                strcpy(msg.message, "STOP");

                esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&msg, sizeof(msg));
            }

            updateDisplay(msgBuffer);
            vibrate(100);
        }
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting...");

    pinMode(MOTOR_PIN, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);

    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 failed");
        return;
    }
    display.setTextColor(SSD1306_WHITE);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW failed");
        return;
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, receiverMacAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

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
