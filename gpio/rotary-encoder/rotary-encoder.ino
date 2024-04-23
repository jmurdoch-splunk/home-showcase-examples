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

// TFT Libraries by Adafruit
// https://learn.adafruit.com/adafruit-gfx-graphics-library/overview
#include <Adafruit_GC9A01A.h> // M5Dial
#include <Adafruit_ST7735.h> // AtomS3
#include <Adafruit_ST7789.h> // M5Stick C
#include <Adafruit_ST77xx.h> 
#include <Adafruit_GFX.h>

// ESP32Encoder by Kevin Harrington
// https://github.com/madhephaestus/ESP32Encoder/
#include <ESP32Encoder.h>
#include <InterruptEncoder.h>

ESP32Encoder encoder;
int value_current = 20;
int value_previous = 20;
int value_max = 30;
int value_min = 12;

// TFT Screens
#if defined(ARDUINO_M5Stack_ATOMS3)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
#endif
#if defined(ARDUINO_M5Stick_C)
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
#endif
#if defined(ARDUINO_STAMP_S3)
Adafruit_GC9A01A tft = Adafruit_GC9A01A(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
#endif

// TFT Variables
int textsize = 12;
int text_x = textsize * 6 * 2;
int text_y = textsize * 8 * 1;
int corner_x = 0;
int corner_y = 0;

TwoWire I2C_INT = TwoWire(0);
TwoWire I2C_EXT = TwoWire(1);

int64_t *scanRotary(void) {
    int64_t *value = NULL;

    value_current = encoder.getCount();
    if (value_current != value_previous) {
        if (value_current > value_max ) {
            encoder.setCount(value_max);
        } else if (value_current  < value_min) {
            encoder.setCount(value_min);
        } else {
            value_previous = value_current;
            value = (int64_t *)malloc(sizeof *value);
            *value = value_current;
        }
    }
    return value;
}

void processRotary(int64_t *value, char *sourcetype) {
    if (value != NULL) {
        // How much memory do we have (for detecting memory leaks)
        Serial.print(F("Free Heap: "));
        Serial.println(ESP.getFreeHeap());
        Serial.println(*value);

        // TFT
        tft.fillRect(corner_x, corner_y, text_x, text_y, ST77XX_RED);
        tft.setCursor(corner_x, corner_y);
        tft.print(*value);

        // Event
        char *event = NULL;
        asprintf(&event, "themostat=%d", *value);
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

        addMetric(ptr, "thermostat", "Thermostat setting", "1", METRIC_GAUGE, 0, 0);
        addDatapoint(ptr, AS_INT, value);
        addDpAttr(ptr,"sensor","Rotary Encoder");

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
    // encoder.attachHalfQuad(GPIO_NUM_41, GPIO_NUM_40);
    encoder.attachSingleEdge(GPIO_NUM_41, GPIO_NUM_40);
    encoder.setCount(value_current);
    // encoder.setFilter(1023);

#ifdef ARDUINO_M5Stack_ATOMS3
    tft.initR(INITR_144GREENTAB);
    tft.setRotation(3);
#endif
#ifdef ARDUINO_M5Stick_C
    tft.init(135, 240); // M5Stick C
#endif
#ifdef ARDUINO_STAMP_S3
    tft.begin(); // M5Dial
#endif
    tft.fillScreen(ST77XX_RED);
    tft.invertDisplay(true);
    tft.setTextSize(textsize);
    corner_x = (tft.width() - text_x + textsize) / 2;
    corner_y = (tft.height() - text_y + textsize) / 2;
    tft.setCursor(corner_x, corner_y);
    tft.print(value_current);
    
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, 230);
}

void loop() {
    uint64_t *value = NULL;
    processRotary(scanRotary(), "thermostat");
}