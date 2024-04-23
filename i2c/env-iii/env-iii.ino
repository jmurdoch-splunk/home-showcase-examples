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

TwoWire I2C_EXT = TwoWire(1);

// This sends OpenTelemetry values
void processSHT30(uint8_t *buf) {
    uint8_t payloadData[MAX_PROTOBUF_BYTES] = { 0 };

    // Create the data store with data structure (default)
    Resourceptr ptr = NULL;
    ptr = addOteldata();

    // Example load metric
    addResAttr(ptr, "service.name", "splunk-home");

    double *temperature = (double *)malloc(sizeof *temperature);
    double *humidity = (double *)malloc(sizeof *humidity);
    *temperature = ((((buf[0] * 256.0) + buf[1]) * 175) / 65535.0) - 45;
    *humidity = (((buf[3] * 256.0) + buf[4]) * 100) / 65535.0;
    free(buf);

    addMetric(ptr, "temperature", "Temperature Reading", "1", METRIC_GAUGE, 0, 0);
    addDatapoint(ptr, AS_DOUBLE, temperature);
    addDpAttr(ptr,"sensor","M5Stack SHT30");

    addMetric(ptr, "humidity", "Humidity Reading", "1", METRIC_GAUGE, 0, 0);
    addDatapoint(ptr, AS_DOUBLE, humidity);
    addDpAttr(ptr,"sensor","M5Stack SHT30");

    // for debug to show whats in the payload
    printOteldata(ptr);
    
    size_t payloadSize = buildProtobuf(ptr, payloadData, MAX_PROTOBUF_BYTES);
    // Send the data if there's something there
    if(payloadSize > 0) {
        // Please set OTEL_SSL in the header
        sendProtobuf(OTEL_HOST, OTEL_PORT, OTEL_URI, OTEL_XSFKEY, payloadData, payloadSize);
    } 

    // Free the data store
    freeOteldata(ptr);
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
    I2C_EXT.begin(GROVE_I2C_SDA, GROVE_I2C_SCL);
    I2C_EXT.beginTransmission(I2C_ADDR_SHT30);
    // Not zero means nothing found at I2C address or an issue
    if (I2C_EXT.endTransmission()) {
	    while(true) {
	        Serial.println("Device not found");
	        delay(1000);
	    }
    }

}

void loop() {
    // How much memory do we have (for detecting memory leaks)
    Serial.print(F("Free Heap: "));
    Serial.println(ESP.getFreeHeap());

    // Delay() between readings
    delay(1000);
    
    // Get the metric - 6 bytes of the temperature and humidity 
    processSHT30(i2cReadWord(I2C_ADDR_SHT30, &I2C_EXT, 0x2C06, 6));
   
}
