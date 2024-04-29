# Home Showcase Examples

In this directory are a list of examples of various M5Stack modules that can 
either log via the HTTP Event Collector (HEC) or send metrics via OTLP.

These have been written to perform the task as simply as possible.

Third-party libraries are used where indicated.

Libraries supplied perform:
- Basic I2C operations
- OTLP protobuf construction and sending
- Splunk HEC JSON construction and sending
- NTP Sync

This has been tested with M5Stack: ATOM, ATOMS3 (inc Lite), M5Stick Plus2 and M5Stamp (M5Dial).

## Example List

| Name | Description | Chip | Type | ID | Output |
|------|-------------|------|------|----|--------|
| Ambient Light | Light Levels | BH1750FV1-TR | I2C | 0x23 | Metric |
| Angle | Variable Resistor Knob | | GPIO | | Metric / Event |
| CO2L  | Temperature, Humidity & CO2 | SCD41 | I2C | 0x62 | Metric |
| Earth Moisture | Soil Moisture | | GPIO | | Metric |
| Env III | Temperature & Humidity | SHT30 | I2C | 0x44 | Metric |
| 24GHz mmWave | Human Static Presence Lite | MR24HPC1 | UART | | Metric / Event |
| IMU | Acceleration & Gyroscope | MPU6886 | I2C | 0x68 | Metric |
| PIR | Infrared Movement | | GPIO | | Metric / Event |
| QR-Code 1.1 | QR & Barcode Reader | SSI-based | UART | | Event |
| RFID2 | RFID Card Reader | WS1850S / RC522 | I2C | 0x28 | Event |
| Rotary Encoder | Circular Dial | | GPIO | | Metric / Event |
| Scales | Scales kit with 4 sensors | HX711 | GPIO | | Metric / Event |
| Switch | Key / Limit Switch | | GPIO | | Event |
| Ultrasonic-I2C | Distance sensor | RCWL-9620 | I2C | 0x57 | Metric |
