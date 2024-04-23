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

// MFRC522_I2C by kkloesener
// https://github.com/kkloesener/MFRC522_I2C
#include <MFRC522_I2C.h>

TwoWire I2C_INT = TwoWire(0);
TwoWire I2C_EXT = TwoWire(1);

MFRC522_I2C * rfid = NULL;

char *scanRFID(void) {
    char *event = NULL;

    if (rfid->PICC_IsNewCardPresent() && rfid->PICC_ReadCardSerial() && rfid->uid.size > 0) {
        // 1 byte = 2 chars in hex + 1 char for NUL terminator
        event = (char *)malloc((sizeof(rfid->uid.uidByte[0]) * rfid->uid.size * 2) + 1);
        for (byte i = 0; i < rfid->uid.size; i++) {
            sprintf(&event[i * 2],"%02x", rfid->uid.uidByte[i]);
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
    I2C_INT.begin(SOCKET_I2C_SDA, SOCKET_I2C_SCL);
    I2C_INT.beginTransmission(I2C_ADDR_RC522);
    // Not zero means nothing found at I2C address or an issue
    if (I2C_INT.endTransmission()) {
        while(true) {
            Serial.println("Device not found");
            delay(1000);
        }
    }
    // Second check for version 0x15 - WS1850S
    if (i2cReadByte(I2C_ADDR_RC522, &I2C_INT, 0x37, 1)[0] != 0x15) {
        while(true) {
            Serial.println("Device not found");
            delay(1000);
        }
    }
    rfid = new MFRC522_I2C(I2C_ADDR_RC522, -1, &I2C_INT);
    rfid->PCD_Init();
}

void loop() {
    // How much memory do we have (for detecting memory leaks)
    Serial.print(F("Free Heap: "));
    Serial.println(ESP.getFreeHeap());

    processEvent(scanRFID(), "rfid");
    delay(1000);
}