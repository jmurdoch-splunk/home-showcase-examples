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

void initMPU6886(uint8_t device, TwoWire *tw) {
    // Power Management 1
    i2cWriteByte(device, tw, 107, 0x00);
    delay(10);
    i2cWriteByte(device, tw, 107, 0x80); // Device Reset
    delay(10);
    i2cWriteByte(device, tw, 107, 0x01); // Full Gyro perf clock source
    delay(10);

    // Acceleration Configuration
    i2cWriteByte(device, tw, 28, 0x10); // +/- 8G
    delay(1);

    // Gyroscope Configuration
    i2cWriteByte(device, tw, 27, 0x18); // +/- 2000dps
    delay(1);

    // Configuration
    i2cWriteByte(device, tw, 26, 0x01); // FIFO replace, no FSYNC, basic low-pass filter
    delay(1);

    // Sample Rate Divider
    i2cWriteByte(device, tw, 25, 0x05); // Divide low-pass filter by 6
    delay(1);

    // Interrupt enable
    i2cWriteByte(device, tw, 56, 0x00); // No interrupts
    delay(1);

    // Accelerometer Configuration 2
    i2cWriteByte(device, tw, 29, 0x00); // 4 samples, nominal rate
    delay(1);

    // User Control
    i2cWriteByte(device, tw, 106, 0x00); // Disable FIFO access
    delay(1);

    // FIFO Enable
    i2cWriteByte(device, tw, 35, 0x00); // Disable FIFO
    delay(1);

    // Interrupt/DataReady PIN & Bypass Enable
    i2cWriteByte(device, tw, 55, 0x22); // Clear interrupt on read
    delay(1);

    // Interrupt Enable
    i2cWriteByte(device, tw, 56, 0x01); // Enable
    delay(100);
}

void processMPU6886(uint8_t *buf) {
    // Process all in one go
    double *ax, *ay, *az, *temp, *gx, *gy, *gz = NULL;
    // Allocate memory
    ax = (double *)malloc(sizeof *ax);
    ay = (double *)malloc(sizeof *ay);
    az = (double *)malloc(sizeof *az);
    temp = (double *)malloc(sizeof *temp);
    gx = (double *)malloc(sizeof *gx);
    gy = (double *)malloc(sizeof *gy);
    gz = (double *)malloc(sizeof *gz);
    
    // Process data to metrics
    *ax = (int16_t)((buf[0] << 8) + buf[1]) * 8.0 / 32768.0;
    *ay = (int16_t)((buf[2] << 8) + buf[3]) * 8.0 / 32768.0;
    *az = (int16_t)((buf[4] << 8) + buf[5]) * 8.0 / 32768.0;
    *temp = ((buf[6] << 8) + buf[7]) / 326.8 + 25.0;
    *gx = (int16_t)((buf[8] << 8) + buf[9]) * 2000.0 / 32768.0;
    *gy = (int16_t)((buf[10] << 8) + buf[11]) * 2000.0 / 32768.0;
    *gz = (int16_t)((buf[12] << 8) + buf[13]) * 2000.0 / 32768.0;

    uint8_t payloadData[MAX_PROTOBUF_BYTES] = { 0 };

    // Create the data store with data structure (default)
    Resourceptr ptr = NULL;
    ptr = addOteldata();

    // Example load metric
    addResAttr(ptr, "service.name", "splunk-home");

    addMetric(ptr, "accel", "Acceleration", "1", METRIC_GAUGE, 0, 0);
    addDatapoint(ptr, AS_DOUBLE, ax);
    addDpAttr(ptr,"sensor","Onboard MPU6886");
    addDpAttr(ptr,"axis","x");
    addDatapoint(ptr, AS_DOUBLE, ay);
    addDpAttr(ptr,"sensor","Onboard MPU6886");
    addDpAttr(ptr,"axis","y");
    addDatapoint(ptr, AS_DOUBLE, az);
    addDpAttr(ptr,"sensor","Onboard MPU6886");
    addDpAttr(ptr,"axis","z");

    addMetric(ptr, "coretemperature", "IMU Core Temperature", "1", METRIC_GAUGE, 0, 0);
    addDatapoint(ptr, AS_DOUBLE, temp);
    addDpAttr(ptr,"sensor","Onboard MPU6886");

    addMetric(ptr, "gyro", "Gyroscope", "1", METRIC_GAUGE, 0, 0);
    addDatapoint(ptr, AS_DOUBLE, gx);
    addDpAttr(ptr,"sensor","Onboard MPU6886");
    addDpAttr(ptr,"axis","x");
    addDatapoint(ptr, AS_DOUBLE, gy);
    addDpAttr(ptr,"sensor","Onboard MPU6886");
    addDpAttr(ptr,"axis","y");
    addDatapoint(ptr, AS_DOUBLE, gz);
    addDpAttr(ptr,"sensor","Onboard MPU6886");
    addDpAttr(ptr,"axis","z");

    free(buf);

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
    I2C_INT.begin(SOCKET_I2C_SDA, SOCKET_I2C_SCL);
    I2C_INT.beginTransmission(I2C_ADDR_MPU6886);
    // Not zero means nothing found at I2C address or an issue
    if (I2C_INT.endTransmission()) {
            while(true) {
                Serial.println("Device not found");
                delay(1000);
            }
    }
    initMPU6886(I2C_ADDR_MPU6886, &I2C_INT);
}

void loop() {
    // How much memory do we have (for detecting memory leaks)
    Serial.print(F("Free Heap: "));
    Serial.println(ESP.getFreeHeap());

    /*
    uint8_t *tempa = i2cReadByte(I2C_ADDR_MPU6886, &I2C_INT, 0x41, 1);
    uint8_t *tempb = i2cReadByte(I2C_ADDR_MPU6886, &I2C_INT, 0x42, 1);

    Serial.println(((*tempa << 8) + *tempb) / 326.8 + 25.0);
    */
    processMPU6886(i2cReadByte(I2C_ADDR_MPU6886, &I2C_INT, 0x3B, 14));
    delay(1000);
}