#include <Arduino.h>
#include <WiFiClientSecure.h>

// Core libraries
#include "src/hwclock/hwclock.h"
#include "src/otel-protobuf/otel-protobuf.h"
#include "src/send-protobuf/send-protobuf.h"
#include "src/splunk-hec/splunk-hec.h"
#include "src/hardware_pins.h"
#include "src/connection_details.h"

// Values for the switch
int value_current, value_previous = 0;

// This compiles the data as an event
void processEvent(int value) {
    // What do we want the message to look like?
    char msg[18];
    sprintf(msg, "SwitchIsOn=%d", value);

    // Now bake it as a HEC message, with a sourcetype
    char *hecjson = makeHECJSON(msg, "switch");

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
    // Delay() between readings
    delay(1000);

    // Read the digital value, inverted for a button/switch
    value_current = digitalRead(GROVE_GPIO_WHITE) ^ 0x1;
    
    // Splunk HEC - An event, sent on significant change in value
    if (value_current != value_previous) {
	    Serial.print("Switch=");
	    Serial.println(value_current);
	    processEvent(value_current);
    }
  
    // Stateful save
    value_previous = value_current;
}
