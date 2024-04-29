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

// For demo
#include "src/QRCode/qrcode.h"

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

void drawQRCode(int qrVer, bool circular_fit, const char *text) {
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(qrVer)];
    uint16_t colour;

    // Build the QRCode
    qrcode_initText(&qrcode, qrcodeData, qrVer, 0, text);

    // Get the QR Dimensions in terms of pixels
    int qrDim = 4 * qrVer + 17;

    // Get the maximal box size that will fit on the screen and borders
    int qrBox = tft->width() > tft->height() ? tft->height() : tft->width();
    // use pythagoras theorem to get the width of a square, if circular
    qrBox = circular_fit ? sqrt(qrBox * qrBox / 2.0) : qrBox;
    int qrTop = (tft->height() - qrBox) / 2;
    int qrLeft = (tft->width() - qrBox) / 2;

    // Get the maximum feasible pixel size
    int qrPix = qrBox / qrDim;

    // Mandatory QR Code gap
    int qrGap = (qrBox - (qrDim * qrPix)) / 2;

    // Background
    tft->fillScreen(ST77XX_WHITE);

    // Now print the QRCode
    for (int x = 0; x < qrDim; x++) {
        for (int y = 0; y < qrDim; y++) { 
            // Decide whether it's a filled pixel
            if (qrcode_getModule(&qrcode, x, y)) {
                colour = ST7735_BLACK;
            } else {
                // probably don't need this if it's all white
                colour = ST7735_WHITE;
            }
            /*
             * Draw the pixel:
             * - using the borders
             * - 
             */
            tft->fillRect(qrLeft + qrGap + (x * qrPix), qrTop + qrGap + (y * qrPix), qrPix, qrPix, colour);
        }
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
}

void loop() {
    drawString("88");
    delay(10000);

    // Print a version 3 QR Code on screen
    drawQRCode(3, isCircle, "splunk.com");
    delay(10000);
}