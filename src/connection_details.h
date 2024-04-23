// Wireless
#define WIFI_SSID "mywifi"
#define WIFI_PSK "mypassphrase"

// NTP
#define NTP_HOST "pool.ntp.org"

// OpenTelemetry Collector
/*
#define OTEL_SSL 1
#define OTEL_HOST "192.168.1.10"
#define OTEL_PORT 4318
#define OTEL_URI "/v1/metrics"
*/

// Splunk O11y Cloud OTLP Endpoint
#define OTEL_SSL 1
#define OTEL_HOST "ingest.us1.signalfx.com"
#define OTEL_PORT 443
#define OTEL_URI "/v2/datapoint/otlp"
#define OTEL_XSFKEY "0123456789abcdef012345"

// Splunk HEC Endpoint
#define HEC_SSL 1
#define HEC_HOST "i-0f7ba0bb820a4a63f.example.fake"
#define HEC_PORT 8088
#define HEC_URI  "/services/collector"
#define HEC_TOKEN "01234567-abcd-abcd-abcd-0123456789ab"
