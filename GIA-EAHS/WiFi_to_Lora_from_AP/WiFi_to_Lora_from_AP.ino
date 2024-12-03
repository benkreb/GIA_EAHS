#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Bu kod, Wi-Fi üzerinden bir sunucudan veri alıp LoRa ağı üzerinden bu veriyi ileten bir sistemi temsil eder.
// Mesaj kayıpsız bir şekilde alınarak istenen zaman aralığında LoRa'ya iletilebilir.

//
// **Yapılacaklar**
// 1. Bridge'den çekilen mesajın kayıpsız şekilde aktarımı sağlanmalıdır.
// 2. Mesaj gönderim sıklığı (saniyede 10 kez yerine), istenilen zaman aralıklarında gerçekleştirilmelidir.
//

// LoRa Configuration
#define RF_FREQUENCY                                868000000 // Hz
#define TX_OUTPUT_POWER                             5         // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 255       // Define the payload size here

char txpacket[BUFFER_SIZE];

bool lora_idle = true;

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
int messageCounter = 0; // Counter for received messages
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
                      true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

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
            messageCounter++;      // Increment the message counter
            Serial.printf("Message Counter: %d\n", messageCounter);

            // Only send every 10th message
            if (messageCounter % 10 == 0) {
                snprintf(txpacket, BUFFER_SIZE, "%s", message.c_str());
                Serial.printf("\r\nSending packet: \"%s\", length: %d\r\n", txpacket, strlen(txpacket));
                Radio.Send((uint8_t *)txpacket, strlen(txpacket));
                lora_idle = false;
            }
        }
        lastSendTime = millis(); // Update the send timestamp
    }
    Radio.IrqProcess();
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
    }
    return "";
}