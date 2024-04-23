#include <Arduino.h>
#include <WiFiClientSecure.h>

// Core libraries
#include "src/hwclock/hwclock.h"
#include "src/otel-protobuf/otel-protobuf.h"
#include "src/send-protobuf/send-protobuf.h"
#include "src/splunk-hec/splunk-hec.h"
#include "src/hardware_pins.h"
#include "src/connection_details.h"

#include "src/i2c-wrapper/i2c-wrapper.h"
#include "src/i2c_devices.h"

TwoWire I2C_INT = TwoWire(0);
TwoWire I2C_EXT = TwoWire(1);

char* scanCode(void) {
    char *event = NULL;

    // Button is pressed, so activate the trigger
    digitalWrite(SOCKET_SPI_MISO, LOW);
    Serial.println("Button Pressed");
    delay(100);
    digitalWrite(SOCKET_SPI_MISO, HIGH);
    
    // Wait for the trigger to kick in
    delay(10);

    // Wait for the scanner to be inactive
    while(!digitalRead(SOCKET_SPI_MOSI)) {
        delay(10);
    }
    delay(500);

    // Data waiting
    if (Serial2.available() > 0) {
        size_t length = Serial2.available();
        // Scanner not active, data waiting
        event = (char *) malloc(length * sizeof(char) + 1);
        // Do we have memory?
        if (event != NULL) {

            Serial2.readBytes(event, length);
            event[length - 1] = '\0';
        }
    }
    return event;
}

void processEvent(char *event, char *sourcetype) {
    if (event != NULL) {
        char *hecjson = makeHECJSON(event, sourcetype);
        Serial.println(hecjson);
        sendHEC(HEC_HOST, HEC_PORT, HEC_URI, HEC_TOKEN, (uint8_t *)hecjson, strlen(hecjson));
        free(event);
        free(hecjson);
    }
}

void setup() {
    // Wait for boot messages to complete
    delay(100);
 
    // USB CDC On Boot: Enabled for Serial to work
    Serial.begin(115200);
    Serial.println(F("Serial Started"));

    // Connect WiFi
    WiFi.begin(WIFI_SSID, WIFI_PSK);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("...Waiting for WiFi..."));
        delay(5000);
    }
    Serial.println(F("WiFi Connected"));

    // NTP Sync - need it for OpenTelemetry
    setHWClock(NTP_HOST);
    Serial.println(F("NTP Synced"));

    /*
     * Now the custom part - here we want to take readings from this
     */
    pinMode(BUTTON, INPUT);

    Serial2.begin(9600, SERIAL_8N1, SOCKET_SPI_SCK,SOCKET_SPI_SS);
    // Trigger - Signal to scan
    pinMode(SOCKET_SPI_MISO, OUTPUT);
    // DLED - Active scan
    pinMode(SOCKET_SPI_MOSI, INPUT);
    // Trigger - active on LOW, so set HIGH
    digitalWrite(SOCKET_SPI_MISO, HIGH);
    delay(20);
    // Read noise
    Serial2.read();

    // Send a command to say we'll use a pulse
    uint8_t buf[9] = { 0x07, 0xC6, 0x04, 0x08, 0x00, 0x8A, 0x02, 0xFE, 0x9B };
    Serial2.write(0x00);
    delay(10);
    Serial2.write(buf, 9);
    delay(60);

    // Consume the ACK and checksum
    uint8_t msg[Serial2.peek() + 2];
    size_t msglen = sizeof(msg);
    Serial2.readBytes(msg, msglen);

}

void loop() {
    // How much memory do we have (for detecting memory leaks)
    // Serial.print(F("Free Heap: "));
    // Serial.println(ESP.getFreeHeap());

    // LOW if button is pressed
    if (digitalRead(BUTTON) == LOW) {
        processEvent(scanCode(), "qrcode");
    }
}