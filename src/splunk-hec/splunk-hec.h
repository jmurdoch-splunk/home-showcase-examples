#ifndef __SPLUNK_HEC_H_
#define __SPLUNK_HEC_H_
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "../connection_details.h"

char *makeHECJSON(const char *event, const char *sourcetype);

int sendHEC(char *host, int port, char *uri, char *hectoken, uint8_t *buf, size_t bufsize) ;

#endif