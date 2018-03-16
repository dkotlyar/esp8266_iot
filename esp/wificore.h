#ifndef WIFICORE_H
#define WIFICORE_H

#include <ESP8266WiFi.h>

const uint8_t performance = 5; // индекс производительности

#define MACTABLE_SIZE 100

// EEPROM
#define SSID_ADDR     0
#define PASSWD_ADDR   (SSID_ADDR + 50)
#define NAME_ADDR     (PASSWD_ADDR + 50)
#define SERVERMEM_ADDR  (NAME_ADDR + 32) // Size = 4 * 5 = 20
#define SERVERMEM_CNT   5
#define WIFIMODE_ADDR   (SERVERMEM_ADDR + SERVERMEM_CNT * 4)
#define SELFPASSWD_ADDR (WIFIMODE_ADDR + 20)

typedef struct {
  byte mac[6];
  IPAddress ip;
  uint32_t time;
} mactable_t;

void wifiSetup();
void wifiLoop();
void wifiSerialKeyValue(String k, String v);
void wifiSerialCmd(String cmd);
boolean wifiConnected();
void printMacTable();
boolean startWiFi();
void wifiStatus();
void changeWifiMode(String mode);
void resetSettings();
void readSettings();
void saveSettings();
void proxyRequest(IPAddress destIP, String apiMethod);
void handleNotFound();
void handlePing();
void handleInfo();
void handleKVGet();
void handleKVSet();
void handleSystemWifi();
void handleChangeServer();
void handleSystemHealthcheck();
void startScanServer();
void stopScanServer();
void setServerMode();
void setServer(IPAddress ip);
void doScanServer();
boolean findServer(IPAddress checkIP);
boolean checkIsServer(IPAddress ip);
void sendHealthcheck();
IPAddress findIPByMac(byte mac[6]);
void updateMacTable(byte mac[6], IPAddress ip);
void parseMacAddress(String macStr, byte mac[6]);
byte strhex2byte(String hex);
boolean macEqual(byte mac1[6], byte mac2[6]);
boolean macEmpty(byte mac[6]);
IPAddress getSubnet(IPAddress ip, IPAddress netmask);
IPAddress getBroadcast(IPAddress subnet, IPAddress netmask);
IPAddress rotateIP(IPAddress ip);
IPAddress nextIP(IPAddress ip);
IPAddress parseIP(String ip);

#endif

