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
#include <Adafruit_GC9A01A.h>   // M5Dial w/ M5Stamp
#include <Adafruit_ILI9341.h>   // M5Core-ESP32
#include <Adafruit_ST7735.h>    // M5AtomS3
#include <Adafruit_ST7789.h>    // M5Stick C
#include <Adafruit_ST77xx.h>
#include <Adafruit_GFX.h>
#include <SPI.h> // For acceleration vs GPIO

// ESP32Encoder by Kevin Harrington
// https://github.com/madhephaestus/ESP32Encoder/
#include <ESP32Encoder.h>
#include <InterruptEncoder.h>

// TFT Screens
#define VSPI_ALTERNATE
#define HSPI_ALTERNATE
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#define VSPI FSPI
#endif
SPIClass *vspi = NULL;
SPIClass *hspi = NULL;

// Is the display circular?
bool isCircle = false;
#if defined(ARDUINO_M5Stack_ATOMS3)
Adafruit_ST7735 *tft = NULL;
#endif
#if defined(ARDUINO_M5Stick_C)
#define HSPI_ALTERNATE GPIO_NUM_13, GPIO_NUM_NC, GPIO_NUM_15, GPIO_NUM_5
Adafruit_ST7789 *tft = NULL;
#endif
#if defined(ARDUINO_STAMP_S3)
#define VSPI_ALTERNATE GPIO_NUM_6, GPIO_NUM_NC, GPIO_NUM_5, GPIO_NUM_7
Adafruit_GC9A01A *tft = NULL;
#endif
#if defined(ARDUINO_M5Stack_Core_ESP32)
Adafruit_ILI9341 *tft = NULL;
#endif

ESP32Encoder encoder;
int value_current = 20;
int value_previous = 20;
int value_max = 30;
int value_min = 12;

TwoWire I2C_INT = TwoWire(0);
TwoWire I2C_EXT = TwoWire(1);

void drawString(char *text) {
    // Text size set
    int textsize = 12;
    tft->setTextSize(textsize);

    // Fill the screen red
    tft->fillScreen(ST77XX_RED);

    // Calculate how to center the text
    int cursor_x = (tft->width() - (textsize * 6 * strlen(text)) + textsize) / 2;
    int cursor_y = (tft->height() - (textsize * 8) + textsize) / 2;
    tft->setCursor(cursor_x, cursor_y);

    tft->print(text);
}

int64_t *scanRotary(void) {
    int64_t *value = NULL;

    value_current = encoder.getCount();
    if (value_current > value_max ) {
        encoder.setCount(value_max);
    } else if (value_current  < value_min) {
        encoder.setCount(value_min);
    } else if (value_current != value_previous) {
        // Update TFT when changed
        char text[sizeof(int)*8+1];
        drawString(itoa(value_current, text, DEC));
        // Bleep when changed
        tone(BUZZER, 1000, 150);
        value_previous = value_current;
    }
    value = (int64_t *)malloc(sizeof *value);
    *value = encoder.getCount();
    return value;
}

void processRotary(int64_t *value, char *sourcetype) {
    if (value != NULL) {
        // How much memory do we have (for detecting memory leaks)
        Serial.print(F("Free Heap: "));
        Serial.println(ESP.getFreeHeap());
        Serial.println(*value);

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

    /*
     * Showcase the display - faster speeds if Hardware SPI used
     */
    // https://docs.espressif.com/projects/arduino-esp32/en/latest/api/spi.html
    vspi = new SPIClass(VSPI);
    hspi = new SPIClass(HSPI);
    // Use custom definitions if set
    vspi->begin(VSPI_ALTERNATE);
    hspi->begin(HSPI_ALTERNATE);
    pinMode(vspi->pinSS(), OUTPUT);  //VSPI SS
    pinMode(hspi->pinSS(), OUTPUT);  //HSPI SS

#ifdef ARDUINO_M5Stack_ATOMS3
    tft = new Adafruit_ST7735(vspi, TFT_CS, TFT_DC, TFT_RST);
    tft->initR(INITR_144GREENTAB);
    tft->setRotation(3);
#endif
#ifdef ARDUINO_M5Stick_C
    tft = new Adafruit_ST7789(hspi, TFT_CS, TFT_DC, TFT_RST);
    tft->init(135, 240);
#endif
#ifdef ARDUINO_STAMP_S3
    tft = new Adafruit_GC9A01A(vspi, TFT_DC, TFT_CS, TFT_RST);
    tft->begin();
    isCircle = true; // M5Dial is round
#endif
#if defined(ARDUINO_M5Stack_Core_ESP32)
    tft = new Adafruit_ILI9341(vspi, TFT_DC, TFT_CS, TFT_RST);
    tft->begin(); 
#endif
    tft->invertDisplay(true);
    // Switch on the backlight    
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, 230);

    // Print something on the screen
    char text[sizeof(int)*8+1];
    drawString(itoa(value_current, text, DEC));
}

void loop() {
    processRotary(scanRotary(), "thermostat");
    delay(1000);
}
