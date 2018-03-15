#ifndef CORE_H
#define CORE_H

#include <ESP8266WiFi.h>

#define NULL nullptr
#define MACTABLE_SIZE 100
#define REGISTER_NAME_LENGTH 32
#define REGISTER_COUNT 32

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

typedef enum { UNUSED = 0, BOOL = 1, BYTE = 2, INT = 3, FLOAT = 4, STRING = 5 } registertype_t;

typedef struct {
  char name[REGISTER_NAME_LENGTH];
  registertype_t type;
  boolean boolValue;
  uint8_t byteValue;
  int32_t intValue;
  float floatValue;
  String stringValue;

  uint8_t hash;
} register_t;

typedef std::function<void(String, String)> callback_kv_t;
typedef std::function<void(String)> callback_cmd_t;

void coreSetup();
void coreLoop();
void printRegisters();
void handleSerialMsg(String& msg);
void registerHandleSerialKeyValue(callback_kv_t callback);
void registerHandleSerialCmd(callback_cmd_t callback);
void createRegister(char name[REGISTER_NAME_LENGTH], registertype_t type);
register_t * getRegister(char name[REGISTER_NAME_LENGTH]);
uint8_t registerHash(register_t reg);
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


