#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <WiFi.h>
#include <HTTPClient.h>

// LoRa Configuration
#define RF_FREQUENCY                                868000000 // Hz
#define TX_OUTPUT_POWER                             10         // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 255       // Define the payload size here
#define PAYLOAD_MAX_SIZE                            200       // Maximum size of each part

char txpacket[BUFFER_SIZE];
bool lora_idle = true;
int packet_number = 0; // Packet counter

// LoRa Events
static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);

// Wi-Fi Configuration
const char *ssid = "EAHS_Routerr";       // Replace with your SSID
const char *password = "Furkan31";      // Replace with your password
const char *serverURL = "http://192.168.4.1"; // Replace with the access point IP

// Timing for periodic operations
unsigned long lastSendTime = 0;
unsigned long sendInterval = 10000; // Send every 10 seconds
String lastMessage = ""; // To store the last fetched message

void setup() {
    Serial.begin(115200);

    // Initialize LoRa
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;

    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                      true, 0, 0, LORA_IQ_INVERSION_ON, 5000); // Timeout increased to 5 seconds

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

void loop() {
    if (lora_idle && (millis() - lastSendTime >= sendInterval)) {
        // Fetch the latest message from the web server
        String message = fetchMessage();

        // Only proceed if a new message is received
        if (message.length() > 0 && message != lastMessage) {
            lastMessage = message; // Update last message
            sendInParts(message);
        }
        lastSendTime = millis(); // Update the send timestamp
    }
    Radio.IrqProcess();
}

void sendInParts(String message) {
    int totalParts = (message.length() + PAYLOAD_MAX_SIZE - 1) / PAYLOAD_MAX_SIZE;
    for (int i = 0; i < totalParts; i++) {
        String partMessage = message.substring(i * PAYLOAD_MAX_SIZE, (i + 1) * PAYLOAD_MAX_SIZE);
        snprintf(txpacket, BUFFER_SIZE, "PART-%d/%d: %s", i + 1, totalParts, partMessage.c_str());
        Serial.printf("\r\nSending packet: \"%s\", length: %d\r\n", txpacket, strlen(txpacket));
        Radio.Send((uint8_t *)txpacket, strlen(txpacket));
        lora_idle = false;

        // Wait for LoRa to finish sending
        while (!lora_idle) {
            Radio.IrqProcess();
            delay(10);
        }
        delay(100); // Small delay between parts
    }
}

void OnTxDone(void) {
    Serial.println("TX done.");
    lora_idle = true;
}

void OnTxTimeout(void) {
    Radio.Sleep();
    Serial.println("TX Timeout.");
    lora_idle = true;
}

// Function to fetch the latest message from the Wi-Fi server
String fetchMessage() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String endpoint = String(serverURL) + "/latest"; // Assuming /latest returns the last message
        http.begin(endpoint);
        int httpResponseCode = http.GET();

        if (httpResponseCode == HTTP_CODE_OK) { // HTTP 200
            String payload = http.getString();
            Serial.println("Received message: " + payload);
            http.end();
            return payload;
        } else {
            Serial.printf("Error fetching message, HTTP code: %d\n", httpResponseCode);
            http.end();
        }
    } else {
        Serial.println("Wi-Fi not connected. Reconnecting...");
        WiFi.reconnect();
        delay(1000); // Wait for reconnect
    }
    return "";
}
