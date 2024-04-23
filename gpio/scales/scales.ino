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

// HX711 Arduino Library by Bogdan Necula
// https://github.com/bogde/HX711
#include <HX711.h>

HX711 * scales = NULL;
float value_current = 0;
float value_previous = 0;

TwoWire I2C_INT = TwoWire(0);
TwoWire I2C_EXT = TwoWire(1);

double *scanScales(void) {
    double *value = NULL;

    if (scales->is_ready()) {
        value_current = scales->get_units(5);
        Serial.println(value_current);
        if (value_current != value_previous) {
            value_previous = value_current;
            value = (double *)malloc(sizeof *value);
            *value = value_current;
        }
    }
    return value;
}

void processRotary(double *value, char *sourcetype) {
    if (value != NULL) {
        // How much memory do we have (for detecting memory leaks)
        Serial.print(F("Free Heap: "));
        Serial.println(ESP.getFreeHeap());
        Serial.println(*value);

        // Event
        char *event = NULL;
        asprintf(&event, "scales=%.02f", *value);
        char *hecjson = makeHECJSON(event, sourcetype);
        Serial.println(hecjson);
        sendHEC(HEC_HOST, HEC_PORT, HEC_URI, HEC_TOKEN, (uint8_t *)hecjson, strlen(hecjson));
        free(event);
        free(hecjson);

        // Metric
        uint8_t payloadData[MAX_PROTOBUF_BYTES] = { 0 };
        Resourceptr ptr = NULL;
        ptr = addOteldata();

        addResAttr(ptr, "service.name", "splunk-home");

        addMetric(ptr, "weight", "Weight in grams", "1", METRIC_GAUGE, 0, 0);
        addDatapoint(ptr, AS_DOUBLE, value);
        addDpAttr(ptr,"sensor","HX711");

        printOteldata(ptr);
        size_t payloadSize = buildProtobuf(ptr, payloadData, MAX_PROTOBUF_BYTES);
        // Send the data if there's something there
        if(payloadSize > 0) {
            // Please set OTEL_SSL in the header
            sendProtobuf(OTEL_HOST, OTEL_PORT, OTEL_URI, OTEL_XSFKEY, payloadData, payloadSize);
        }
        freeOteldata(ptr);
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
    scales = new HX711();
    scales->begin(GROVE_GPIO_WHITE, GROVE_GPIO_YELLOW);
    scales->set_scale(24.0f);
    scales->tare();
}

void loop() {
    double *value = NULL;
    processRotary(scanScales(), "scales");
}
