/*
 * Basic example using an angle sensor (variable resistor).
 *
 * Setup:
 * - Select the appropriate board type
 *     - Set "USB CDC On Boot" to "Enabled" for Serial Access
 * - Select Serial Port
 *     - /dev/ttyACM0
 * - Select Programmer
 *     - Esptool
 * 
 * To then connect to the device:
 *     cu -s 115200 -l /dev/ttyACM0 
 */
#include <Arduino.h>
#include <WiFiClientSecure.h>

// Core libraries
#include "src/hwclock/hwclock.h"
#include "src/otel-protobuf/otel-protobuf.h"
#include "src/send-protobuf/send-protobuf.h"
#include "src/splunk-hec/splunk-hec.h"
#include "src/hardware_pins.h"
#include "src/connection_details.h"

// Values for the Angle Reader (0-4095)
int value_current, value_previous = 0;
int value_threshold = 50; 

// This sends OpenTelemetry values
void processMetric(int value) {
    uint8_t payloadData[MAX_PROTOBUF_BYTES] = { 0 };

    // Create the data store with data structure (default)
    Resourceptr ptr = NULL;
    ptr = addOteldata();

    // Example load metric
    addResAttr(ptr, "service.name", "splunk-home");

    uint64_t *dialvalue = (uint64_t *)malloc(sizeof(uint64_t));
    *dialvalue = value;
    addMetric(ptr, "angle", "Angle Reading", "1", METRIC_GAUGE, 0, 0);
    addDatapoint(ptr, AS_INT, dialvalue);
    addDpAttr(ptr,"sensor","M5Stack Angle Sensor");

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

// This compiles the data as an event
void processEvent(int value) {
    // What do we want the message to look like?
    char msg[18];
    sprintf(msg, "CurrentAngle=%d", value);

    // Now bake it as a HEC message, with a sourcetype
    char *hecjson = makeHECJSON(msg, "anglereading");

    // Send to HEC
    sendHEC(HEC_HOST, HEC_PORT, HEC_URI, HEC_TOKEN, (uint8_t *) hecjson, strlen(hecjson));
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
    pinMode(GROVE_GPIO_WHITE, INPUT);
}

void loop() {
    // How much memory do we have (for detecting memory leaks)
    Serial.print(F("Free Heap: "));
    Serial.println(ESP.getFreeHeap());

    // Delay() between readings
    delay(1000);
    
    // Get the metric
    value_current = ~analogRead(GROVE_GPIO_WHITE) & 0x0FFF;
    Serial.print(F("Angle Reading: "));
    // 12bit ADCresolution
    Serial.println(value_current);

    // OTLP - A metric, sent always
    processMetric(value_current);
    
    // Splunk HEC - An event, sent on significant change in value
    if (abs(value_current - value_previous) > value_threshold) {
        processEvent(value_current);
    }
  
    // Stateful save
    value_previous = value_current;
}
