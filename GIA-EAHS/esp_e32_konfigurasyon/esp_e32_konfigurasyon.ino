#include "LoRa_E32.h"
#include "HardwareSerial.h"

// Define module pins
#define PIN_M0 4       // M0 pin of LoRa module
#define PIN_M1 5       // M1 pin of LoRa module
#define PIN_AUX 2      // AUX pin of LoRa module
#define RX_PIN 16      // ESP32 UART2 RX
#define TX_PIN 17      // ESP32 UART2 TX

// Initialize hardware serial port (Serial2 used for this example)
HardwareSerial mySerial(2);  // Using UART2

// Initialize LoRa module
LoRa_E32 e32ttl(RX_PIN, TX_PIN, &mySerial, PIN_AUX, UART_BPS_RATE_9600);

void printParameters(struct Configuration configuration);

void setup() {
    // Set M0, M1 pins as OUTPUT
    pinMode(PIN_M0, OUTPUT);
    pinMode(PIN_M1, OUTPUT);

    // Set AUX pin as INPUT (optional for monitoring module status)
    pinMode(PIN_AUX, INPUT);

    // Set module to Configuration Mode (M0 = HIGH, M1 = HIGH)
    digitalWrite(PIN_M0, HIGH);
    digitalWrite(PIN_M1, HIGH);

    // Start Serial for debugging
    Serial.begin(115200);
    delay(500);

    // Start the serial communication with the LoRa module
    mySerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    // Initialize LoRa module
    Serial.println("Initializing LoRa module...");
    e32ttl.begin();

    // Wait for AUX pin to indicate ready state
    while (digitalRead(PIN_AUX) == HIGH) {
        delay(10);
    }

    // Get current configuration
    Serial.println("Getting current configuration...");
    ResponseStructContainer c = e32ttl.getConfiguration();

    if (c.status.code == SUCCESS) {
        // If successful, read configuration
        Configuration configuration = *(Configuration*)c.data;

        Serial.println("Current configuration:");
        printParameters(configuration);

        // Modify configuration for 868 MHz
        configuration.ADDL = 0x01; // Device address low byte
        configuration.ADDH = 0x00; // Device address high byte
        configuration.CHAN = 0x00; // Channel 0 (868 MHz base frequency)

        configuration.OPTION.fec = FEC_1_ON; // Enable Forward Error Correction
        configuration.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION; // Transparent mode
        configuration.OPTION.transmissionPower = POWER_20; // Max power (20 dBm)
        configuration.SPED.uartBaudRate = UART_BPS_9600; // UART baud rate 9600
        configuration.SPED.airDataRate = AIR_DATA_RATE_010_24; // Air data rate 2.4Kbps

        // Save new configuration
        ResponseStatus rs = e32ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
        if (rs.code == SUCCESS) {
            Serial.println("Configuration updated successfully.");
        } else {
            Serial.println("Failed to update configuration.");
        }

        // Print updated configuration
        Serial.println("Updated configuration:");
        printParameters(configuration);
    } else {
        Serial.println("Failed to retrieve configuration.");
    }

    // Close response container
    c.close();

    // Set module to Normal Mode (M0 = LOW, M1 = LOW)
    digitalWrite(PIN_M0, LOW);
    digitalWrite(PIN_M1, LOW);
    delay(100); // Allow time for mode transition

    Serial.println("LoRa module is now in Normal Mode.");
}

void loop() {
    // Add your data transmission or reception code here
}

void printParameters(struct Configuration configuration) {
    Serial.println("----------------------------------------");
    Serial.print(F("HEAD : ")); Serial.println(configuration.HEAD, HEX);
    Serial.print(F("ADDH : ")); Serial.println(configuration.ADDH, HEX);
    Serial.print(F("ADDL : ")); Serial.println(configuration.ADDL, HEX);
    Serial.print(F("CHAN : ")); Serial.print(configuration.CHAN, DEC); 
    Serial.print(" -> "); Serial.println(configuration.getChannelDescription());
    Serial.print(F("FEC  : ")); Serial.println(configuration.OPTION.getFECDescription());
    Serial.print(F("Power: ")); Serial.println(configuration.OPTION.getTransmissionPowerDescription());
    Serial.println("----------------------------------------");
}
