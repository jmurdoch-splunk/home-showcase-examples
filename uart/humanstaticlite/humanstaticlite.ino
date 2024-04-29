#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HardwareSerial.h>

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

const unsigned char cmd_standard[10] = {0x53, 0x59, 0x08, 0x00, 0x00, 0x01, 0x00, 0xB5, 0x54, 0x43}; // switch off Open Underlying Message
const unsigned char cmd_underlying_open[10] = {0x53, 0x59, 0x08, 0x00, 0x00, 0x01, 0x01, 0xB6, 0x54, 0x43};

// XIAO_ESP32C3
HardwareSerial HSPL(1);

// https://files.seeedstudio.com/wiki/mmWave-radar/MR24HPC1_User_Manual-V1.5.pdf
uint8_t *scanRadar(void)
{
  while (HSPL.available() > 8)
  {
    if (HSPL.read() == 0x53)
    {
      if (HSPL.read() == 0x59)
      {
        // Header
        uint8_t ctrl = HSPL.read();
        uint8_t cmd = HSPL.read();
        uint16_t len = (HSPL.read() << 8);
        len |= HSPL.read();

        // Body
        uint8_t *buf = (uint8_t *)malloc(len + 4); 
        buf[0] = ctrl;
        buf[1] = cmd; 
        buf[2] = (len >> 8);
        buf[3] = (len & 0xFF);
        for (int i = 4; i < len + 4; i++)
        {
          buf[i] = HSPL.read();
        }

        // Read tail
        uint8_t cksum = HSPL.read();

        // Throw the last few bytes away - we'll use the checksum
        HSPL.read();
        HSPL.read();

        // Checksum validation
        uint8_t bytesum = 0xAC;
        for (int i = 0; i < len + 4; i++)
        {
          bytesum += buf[i];
        }

        // Does this check out?
        if (cksum == bytesum)
        {
          return buf;
        }
      }
    }
  }
  return NULL;
}

void parseRadar(uint8_t *buf)
{
  if (buf != NULL)
  {
    uint16_t len = (buf[2] << 8);
    len |= buf[3];

    if (buf[0] == 0x80)
    {
      /*
       * Event-based motion sensing w/ regular energy reports
       */
      char *event = NULL;

      switch (buf[1])
      {
      case 0x01:
        switch (buf[4])
        {
          case 0x01:
            asprintf(&event, "Space: Presence");
            break;
          default:
            asprintf(&event, "Space: Vacant");
            break;
        }
      case 0x02:
        switch (buf[4])
        {
        case 0x01:
          asprintf(&event, "Motion: Stationary");
          break;
        case 0x02:
          asprintf(&event, "Motion: Moving");
          break;
        default:
          asprintf(&event, "Motion: Vacant");
          break;
        }
        break;
      case 0x0B:
        switch (buf[4])
        {
        case 0x01:
          asprintf(&event, "Direction: Advancing");
          break;
        case 0x02:
          asprintf(&event, "Direction: Retreating");
          break;
        default:
          asprintf(&event, "Direction: Stationary");
          break;
        }
        break;
      case 0x03:
        switch (buf[4])
        {
        case 0x00:
          asprintf(&event, "Energy: Vacant");
          break;
        case 0x01:
          asprintf(&event, "Energy: Stationary");
          break;
        default:
          // change from 2-100 to 1-99
          asprintf(&event, "Energy: %d", buf[4] - 1);
          break;
        }
        break;
      
      default:
        break;
      }
      processEvent(event, "mmwave");
    }
    else if (buf[0] == 0x08 && buf[1] == 0x01)
    {
      /*
       * Metric-based statistics on radar:
       * - Static & Motion Energy (0-100)
       * - Static & Motion Distance (0-4 meters)
       * - Motion Speed (0-4 meters)
       */
      processUnderlyingOpen(buf);
    }
    else if (buf[0] == 0x01 && buf[1] == 0x01) {
      Serial.println(F("Heartbeat"));
    }
    else
    {
      for (int i = 0; i < len + 4; i++)
      {
        Serial.print(buf[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
    }
  }
}

void processUnderlyingOpen(uint8_t *buf) {

    Serial.print("StaticEnergy=");
    Serial.print(buf[4]);
    Serial.print(", StaticDistance=");
    Serial.print(buf[5]);
    Serial.print(", MotionEnergy=");
    Serial.print(buf[6]);
    Serial.print(", MotionDistance=");
    Serial.print(buf[7]);
    Serial.print(", MotionSpeed=");
    // Convert 0-9/10/11-20 unsigned, to +/-10
    Serial.print((int8_t)buf[8] - 10);
    Serial.println();
    
    // Process all in one go
    int64_t *senergy, *menergy = NULL;
    double *sdist, *mdist, *mspeed = NULL;

    // Allocate memory
    senergy = (int64_t *)malloc(sizeof *senergy);
    menergy = (int64_t *)malloc(sizeof *menergy);

    sdist = (double *)malloc(sizeof *sdist);
    mdist = (double *)malloc(sizeof *mdist);
    mspeed = (double *)malloc(sizeof *mspeed);

    *senergy = buf[4];
    *sdist = buf[5] * 0.5;
    *menergy = buf[6];
    *mdist = buf[7] * 0.5;
    *mspeed = (buf[8] - 10) * 0.5;

    uint8_t payloadData[MAX_PROTOBUF_BYTES] = { 0 };

    // Create the data store with data structure (default)
    Resourceptr ptr = NULL;
    ptr = addOteldata();

    // Example load metric
    addResAttr(ptr, "service.name", "splunk-home");

    addMetric(ptr, "static-energy", "Measurement of micro-motion noise of static person", "1", METRIC_GAUGE, 0, 0);
    addDatapoint(ptr, AS_INT, senergy);
    addDpAttr(ptr,"sensor","MR24HPC1");

    addMetric(ptr, "static-distance", "Distance to micro-motion noise of static person", "1", METRIC_GAUGE, 0, 0);
    addDatapoint(ptr, AS_DOUBLE, sdist);
    addDpAttr(ptr,"sensor","MR24HPC1");

    addMetric(ptr, "motion-energy", "Measurement of micro-motion noise of moving person", "1", METRIC_GAUGE, 0, 0);
    addDatapoint(ptr, AS_INT, menergy);
    addDpAttr(ptr,"sensor","MR24HPC1");

    addMetric(ptr, "motion-distance", "Distance to micro-motion noise of moving person", "1", METRIC_GAUGE, 0, 0);
    addDatapoint(ptr, AS_DOUBLE, mdist);
    addDpAttr(ptr,"sensor","MR24HPC1");

    addMetric(ptr, "motion-speed", "Speed of micro-motion noise of moving person", "1", METRIC_GAUGE, 0, 0);
    addDatapoint(ptr, AS_DOUBLE, mspeed);
    addDpAttr(ptr,"sensor","MR24HPC1");

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

void processEvent(char *event, char *sourcetype)
{
  if (event != NULL)
  {
    char *hecjson = makeHECJSON(event, sourcetype);
    Serial.println(hecjson);
    sendHEC(HEC_HOST, HEC_PORT, HEC_URI, HEC_TOKEN, (uint8_t *)hecjson, strlen(hecjson));
    free(event);
    free(hecjson);
  }
}

void setup()
{
  // Wait for boot messages to complete
  delay(100);

  // USB CDC On Boot: Enabled for Serial to work
  Serial.begin(115200);
  Serial.println(F("Serial Started"));

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  while (WiFi.status() != WL_CONNECTED)
  {
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
  HSPL.begin(115200, SERIAL_8N1, 4, 5);
  // HSPL.write(cmd_standard, sizeof(cmd_standard));
  HSPL.write(cmd_underlying_open, sizeof(cmd_underlying_open));
}

void loop()
{
  parseRadar(scanRadar());
}