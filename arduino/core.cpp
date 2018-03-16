#include "core.h"

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include <ESP8266Ping.h>

const uint8_t performance = 5; // индекс производительности

mactable_t mactable[MACTABLE_SIZE];

char ssid[50];
char password[50];
WiFiMode_t wifimode;
char selfssid[20];
char selfpassword[20];

char name[32];

ESP8266WebServer server(80);

IPAddress serverIP;
IPAddress ipMemory[SERVERMEM_CNT];
boolean isServer = false;
boolean scanServer = false;
boolean scanServerMemory = false;
boolean scanServerList = false;
IPAddress scanIP;
int scanMemoryIPIndex;

IPAddress localIP;
IPAddress netmask;
IPAddress subnet;
IPAddress broadcastIP;

String serialBuffer = "";
DynamicJsonBuffer jsonBuffer(500);

register_t registerBuffer[REGISTER_COUNT];

callback_kv_t userSerialKeyValueCallback = NULL;
callback_cmd_t userSerialCommandCallback = NULL;

void coreSetup() {
  serialBuffer.reserve(255);
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println();

  EEPROM.begin(512);

  readSettings();
  if (strcmp(ssid, "") != 0) {
    startWiFi();
  }

  byte mac[6];
  WiFi.macAddress(mac);

  ("ESP_" + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX)).toCharArray(selfssid, 20);

  WiFi.mode(wifimode);
  WiFi.softAP(selfssid, selfpassword);

  server.onNotFound(handleNotFound);
  server.on("/ping", handlePing);
  server.on("/info", handleInfo);
  server.on("/kv/get", handleKVGet);
  server.on("/kv/set", handleKVSet);
  server.on("/system/wifi", handleSystemWifi);
  server.on("/system/healthcheck", handleSystemHealthcheck);
  server.on("/system/changeServer", handleChangeServer);
  server.begin();
}

void coreLoop() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      handleSerialMsg(serialBuffer);
      serialBuffer = "";
    } else {
      serialBuffer += inChar;
    }
  }

  server.handleClient();

  if (scanServer) {
    doScanServer();
  }
  else {
    if (!isServer) {
      static uint32_t lastHealthcheck = millis();
      if (millis() - lastHealthcheck > 30000) {
        sendHealthcheck();
        lastHealthcheck = millis();
      }
    }
  }
  
  printRegisters();
}

void printRegisters() {
  for (int i = 0; i < REGISTER_COUNT; i++) {
    if (registerBuffer[i].type != UNUSED) {
      uint8_t hash = registerHash(registerBuffer[i]);
      if (registerBuffer[i].hash != hash) {
        Serial.print("?register:");
        Serial.print(registerBuffer[i].name);
        Serial.print("=");
        Serial.println(registerGetValue(&registerBuffer[i], true));
        registerBuffer[i].hash = hash;
      }
    }
  }
}

void handleSerialMsg(String& msg) {
  if (msg.indexOf("?") != 0) {
    return;
  }

  if (msg.indexOf("=") != -1) {
    String key = msg.substring(1, msg.indexOf("="));
    String value = msg.substring(msg.indexOf("=") + 1);

    if (key == "ssid") {
      value.toCharArray(ssid, 50);
    } else if (key == "password") {
      value.toCharArray(password, 50);
    } else if (key == "wifimode") {
      changeWifiMode(value);
    } else if (key.startsWith("register:")) {
      String regName = key.substring(key.indexOf(":")+1);
      register_t* reg = getRegister(regName);
      if (reg != NULL) {
        registerSetValue(reg, value);
        reg->hash = registerHash(*reg);
        Serial.print(key);
        Serial.print("=");
        Serial.print(value);
        Serial.println(" OK");
      } else {
        Serial.println("Unknown register");
      }
    } else if (key == "newregister") {
      String regName = value.substring(0, value.indexOf(":"));
      value = value.substring(value.indexOf(":") + 1);
      String regType = value.substring(0, value.indexOf(":"));
      String regAccess = value.substring(value.indexOf(":") + 1);
      createRegister(regName, registerType(regType), registerAccess(regAccess));
      register_t* newReg = getRegister(regName);
      Serial.print("Register created with name \"");
      Serial.print(newReg->name);
      Serial.print("\", type=");
      Serial.print(registerType(newReg->type));
      Serial.print(", access=");
      Serial.println(registerAccess(newReg->access));
    }
    else {
      if (userSerialKeyValueCallback != NULL)
        userSerialKeyValueCallback(key, value);
    }

  } else {
    String cmd = msg.substring(1);

    if (cmd == "startwifi") {
      startWiFi();
    } else if (cmd == "wifistatus") {
      wifiStatus();
//    } else if (cmd == "wifisave") {
//      saveSettings();
//      Serial.println("Settings saved");
    } else if (cmd == "resetsettings") {
      resetSettings();
      Serial.println("Settings was reset to default");
    } else if (cmd == "printmactable") {
      printMacTable();
    } else if (cmd == "forceserver") {
      setServerMode();
    }
    else {
      if (userSerialCommandCallback != NULL)
        userSerialCommandCallback(cmd);
    }
  }
}

void registerHandleSerialKeyValue(callback_kv_t callback) {
  userSerialKeyValueCallback = callback;
}

void registerHandleSerialCmd(callback_cmd_t callback) {
  userSerialCommandCallback = callback;
}

void createRegister(String name, registertype_t type, registeraccess_t access) {
  createRegister(name.c_str(), type, access);
}

void createRegister(const char name[REGISTER_NAME_LENGTH], registertype_t type, registeraccess_t access) {
  for (int i = 0; i < REGISTER_COUNT; i++) {
    if (registerBuffer[i].type == UNUSED) {
      memcpy(registerBuffer[i].name, name, REGISTER_NAME_LENGTH);
      registerBuffer[i].type = type;
      registerBuffer[i].access = access;
      return;
    }
  }
}

register_t * getRegister(String name) {
  return getRegister(name.c_str());
}

register_t * getRegister(const char name[REGISTER_NAME_LENGTH]) {
  for (int i = 0; i < REGISTER_COUNT; i++) {
    if (strcmp(registerBuffer[i].name, name) == 0) {
      return &registerBuffer[i];
    }
  }
  
  return NULL;
}

String registerGetValue(register_t* reg, boolean quoteString) {
  switch(reg->type) {
    case BOOL: return String(reg->boolValue);
    case BYTE: return String(reg->byteValue);
    case INT: return String(reg->intValue);
    case FLOAT: String(reg->floatValue);
    case STRING: 
      if (quoteString)
        return "\"" + reg->stringValue + "\"";
      else
        return reg->stringValue;
    default: if (quoteString) return "\"\""; else return "";
  }
}

void registerSetValue(register_t* reg, String valueStr) {
  switch(reg->type) {
    case BOOL:
      valueStr.toLowerCase();
      reg->boolValue = (valueStr == "yes" || valueStr == "on" || valueStr == "true" || valueStr == "1");
      return;
    case BYTE: reg->byteValue = (uint8_t)valueStr.toInt(); return;
    case INT: reg->intValue = valueStr.toInt(); return;
    case FLOAT: reg->floatValue = valueStr.toFloat(); return;
    case STRING: reg->stringValue = valueStr; return;
    default: 
      return;
  }
}

uint8_t registerHash(register_t reg) {
//  boolean boolValue;
//  uint8_t byteValue;
//  int32_t intValue;
//  float floatValue;
//  String stringValue;

  uint8_t * regMem = (uint8_t*)malloc(sizeof(boolean) + sizeof(uint8_t) + sizeof(int32_t) + sizeof(float));
  memcpy(regMem, &reg.boolValue, sizeof(boolean));
  memcpy(regMem + sizeof(boolean), &reg.byteValue, sizeof(uint8_t));
  memcpy(regMem + sizeof(boolean) + sizeof(uint8_t), &reg.intValue, sizeof(int32_t));
  memcpy(regMem + sizeof(boolean) + sizeof(uint8_t) + sizeof(int32_t), &reg.floatValue, sizeof(float));

  uint8_t hash = 0;
  uint8_t *regCur = regMem;
  for (int i = 0; i < sizeof(boolean) + sizeof(uint8_t) + sizeof(int32_t) + sizeof(float); i++) {
    hash ^= *regCur;
    regCur++;
  }

  for (int i = 0; i < reg.stringValue.length(); i++) {
    hash ^= reg.stringValue[i];
  }

  free(regMem);

  return hash;
}

registertype_t registerType(String regType) {
  if (regType == "bool") {
    return BOOL;
  } else if (regType == "byte") {
    return BYTE;
  } else if (regType == "int") {
    return INT;
  } else if (regType == "float") {
    return FLOAT;
  } else if (regType == "string") {
    return STRING;
  } else {
    return UNUSED;
  }
}

String registerType(registertype_t regType) {
  switch (regType) {
    case BOOL: return "bool";
    case BYTE: return "byte";
    case INT: return "int";
    case FLOAT: return "float";
    case STRING: return "string";
    default: return "unused";
  }
}

registeraccess_t registerAccess(String regAccess) {
  registertype_t access;
  if (regAccess == "rw" || regAccess == "readwrite" || regAccess == "read-write") {
    return READWRITE;
  } else {
    return READONLY;
  }
}

String registerAccess(registeraccess_t regAccess) {
  switch (regAccess) {
    case READWRITE: return "read-write";
    default: return "read-only";
  }
}

void printMacTable() {
  for (int i = 0; i < MACTABLE_SIZE; i++) {
    if (!macEmpty(mactable[i].mac)) {
      Serial.print("MAC: ");
      for (int j = 0; j < 6; j++) {
        if (j > 0) Serial.print(":");
        Serial.print(mactable[i].mac[j], HEX);
      }
      Serial.println();
      Serial.print("IP: ");
      Serial.println(mactable[i].ip);
      Serial.print("Last alive: ");
      Serial.print((millis() - mactable[i].time) / 1000);
      Serial.println("(s)");
    } else {
      Serial.print("Total records: ");
      Serial.println(i);
      return;
    }
  }
}

boolean startWiFi() {
  if (wifimode != WIFI_STA && wifimode != WIFI_AP_STA) {
    Serial.println("Couldn't start WiFi. Change WiFi mode");
    return false;
  }

  WiFi.disconnect();
  WiFi.begin(ssid, password);
  Serial.println("Start WiFi");

  auto repeat = 30;
  while (WiFi.status() != WL_CONNECTED && repeat > 0) {
    delay(500);
    Serial.print(".");
    repeat--;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK");
    saveSettings();
    
    localIP = WiFi.localIP();
    netmask = WiFi.subnetMask();
    subnet = getSubnet(localIP, netmask);
    broadcastIP = getBroadcast(subnet, netmask);
    startScanServer();

    return true;
  } else {
    Serial.println(" ERR");

    return false;
  }
}

void wifiStatus() {
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  //  Serial.println(password);
  for (int i = 0; password[i] != '\0' && i < 50; i++) {
    Serial.print("*");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected: Yes");
    Serial.print("IP: ");
    Serial.println(localIP);
  } else {
    Serial.println("Connected: No");
  }
}

void changeWifiMode(String mode) {
  if (mode == "sta") {
    wifimode = WIFI_STA;
  } else if (mode == "ap") {
    wifimode = WIFI_AP;
  } else {
    wifimode = WIFI_AP_STA;
  }
  WiFi.mode(wifimode);
}

void resetSettings() {
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

void readSettings() {
  for (int i = 0; i < 50; i++) {
    ssid[i] = EEPROM.read(SSID_ADDR + i);
    password[i] = EEPROM.read(PASSWD_ADDR + i);
  }

  wifimode = (WiFiMode_t)EEPROM.read(WIFIMODE_ADDR);
  if (wifimode != WIFI_STA && wifimode != WIFI_AP && wifimode != WIFI_AP_STA) {
    wifimode = WIFI_AP;
  }
  
  for (int i = 0; i < 20; i++) {
    selfpassword[i] = EEPROM.read(SELFPASSWD_ADDR + i);
  }
  
}

void saveSettings() {
  for (int i = 0; i < 50; i++) {
    EEPROM.write(SSID_ADDR + i, ssid[i]);
    EEPROM.write(PASSWD_ADDR + i, password[i]);
  }

  EEPROM.write(WIFIMODE_ADDR, (uint8_t)wifimode);

  for (int i = 0; i < 20; i++) {
    EEPROM.write(SELFPASSWD_ADDR + i, selfpassword[i]);
  }
  
  EEPROM.commit();
}

void proxyRequest(IPAddress destIP, String apiMethod) {
  if (destIP == IPAddress(0,0,0,0)) {
    server.send(200, "application/json", "{\"result\": \"ERR\"}");
  } else {
    HTTPClient http;
    http.begin("http://" + destIP.toString() + apiMethod);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String json = http.getString();
      server.send(200, "application/json", json);
    } else {
      server.send(200, "application/json", "{\"result\": \"ERR\"}");
    }
    http.end();
  }
}

void handleNotFound() {
  server.send(404, "application/json", "{\"result\": \"ERR\", \"msg\": \"Unknown request\"}");
}

void handlePing() {
  server.send(200, "application/json",
              String("{\"result\": \"OK\",") +
              String("\"name\": \"" + String(name) + "\",") +
              String("\"server\": \"" + serverIP.toString() + "\",") +
              String("\"my\": \"" + localIP.toString() + "\"") +
              String("}")
             );
}

void handleInfo() {
  server.send(200, "application/json", 
    String("{\"result\":\"OK\",") +
    String("\"performance\":" + String(performance) + ",") +
    String("\"mac\":\"" + String(WiFi.macAddress()) + "\",") + 
    String("\"name\":\"" + String(name) + "\",") + 
    String("\"isserver\":" + String(isServer ? "true" : "false") + ",") + 
    String("\"powertime\":" + String(millis() / 1000) + ",") + 
    String("\"batterystatus\":\"AC\",") +
    String("\"batterylevel\":100") +
    String("}")
  );
}

void handleKVGet() {
  String regName = server.arg("register");
  String macStr = regName.substring(0, regName.indexOf("/"));
  String registerName = regName.substring(regName.indexOf("/") + 1);

  byte mac[6];
  parseMacAddress(macStr, mac);
  byte selfmac[6];
  WiFi.macAddress(selfmac);

  if (macEqual(mac, selfmac)) {
    register_t * reg = getRegister(registerName);
    if (reg == NULL) {
      server.send(200, "application/json", "{\"result\":\"ERR\", \"msg\": \"Register not found\"}");
      return;
    }
    
    String value = registerGetValue(reg, true);
    server.send(200, "application/json",
                String("{\"result\": \"OK\",") +
                String("\"register\": \"" + regName + "\", ") +
                String("\"value\":" + value) +
                String("}")
               );
  } else {
    IPAddress destIP = findIPByMac(mac);
    proxyRequest(destIP, "/kv/get?register=" + regName);
  }
}

void handleKVSet() {
  String regName = server.arg("register");
  String valueStr = server.arg("value");
  String macStr = regName.substring(0, regName.indexOf("/"));
  String registerName = regName.substring(regName.indexOf("/") + 1);

  byte mac[6];
  parseMacAddress(macStr, mac);
  byte selfmac[6];
  WiFi.macAddress(selfmac);
  
  if (macEqual(mac, selfmac)) {
    register_t * reg = getRegister(registerName);
    if (reg == NULL) {
      server.send(200, "application/json", "{\"result\":\"ERR\", \"msg\": \"Register not found\"}");
      return;
    }
    if (reg->access == READONLY) {
      server.send(200, "application/json", "{\"result\":\"ERR\", \"msg\": \"Register read-only access mode\"}");
    }

    registerSetValue(reg, valueStr);    
    server.send(200, "application/json",
                String("{\"result\": \"OK\",") +
                String("\"register\": \"" + regName + "\", ") +
                String("\"value\": \"" + valueStr + "\"") +
                String("}")
               );
  } else {
    IPAddress destIP = findIPByMac(mac);
    proxyRequest(destIP, "/kv/set?register=" + regName + "&value=" + valueStr);
  }
}

void handleSystemWifi() {
  String ssidArg = server.arg("ssid");
  String passwordArg = server.arg("password");
  ssidArg.toCharArray(ssid, 50);
  passwordArg.toCharArray(password, 50);
  
  server.send(200, "application/json", "{\"result\": \"OK\"}");

  if (wifimode == WIFI_AP || wifimode == WIFI_AP_STA) {
    wifimode = WIFI_AP_STA;
  } else {
    wifimode = WIFI_STA;
  }
  
  WiFi.mode(wifimode);
  startWiFi();
}

void handleChangeServer() {
  String ipArg = server.arg("ip");
  String performanceArg = server.arg("performance");

  IPAddress ip = parseIP(ipArg);

  if (isServer) {
    uint8_t performanceNewServer = performanceArg.toInt();
    if (performanceNewServer > performance) {
      isServer = false;
      serverIP = ip;
      server.send(200, "application/json", 
        String("{\"result\":\"OK\",") + 
        String("\"newserver\":\"" + serverIP.toString() + "\"") + 
        String("}")
      );
    } else {
      server.send(200, "application/json", "{\"result\": \"ERR\", \"msg\": \"Low performance\"}");
    }
  } else {
    serverIP = ip;
    server.send(200, "application/json", 
      String("{\"result\":\"OK\",") + 
      String("\"newserver\":\"" + serverIP.toString() + "\"") + 
      String("}")
    );
  }  
}

void handleSystemHealthcheck() {
  if (isServer) {
    String macArg = server.arg("mac");
    String ipArg = server.arg("ip");
    byte mac[6];
    parseMacAddress(macArg, mac);
    updateMacTable(mac, parseIP(ipArg));
    server.send(200, "application/json", "{\"result\": \"OK\"}");
  } else {
    server.send(200, "application/json", 
      String("{\"result\":\"ERR\",") + 
      String("\"server\":\"" + serverIP.toString() + "\"") +
      String("}")
    );
  }
}

void startScanServer() {
  scanServer = true;
  
  Serial.println("Start scan server");
  Serial.print("My IP:   ");
  Serial.println(localIP);
  Serial.print("Netmask: ");
  Serial.println(netmask);
  Serial.print("Subnet:  ");
  Serial.println(subnet);
  Serial.print("Brdcast: ");
  Serial.println(broadcastIP);

  for (int i = 0; i < SERVERMEM_CNT; i++) {
    byte ip[4];
    for (int j = 0; j < 4; j++) {
      ip[j] = EEPROM.read(SERVERMEM_ADDR + i * 4 + j);
    }
    ipMemory[i] = IPAddress(ip[0], ip[1], ip[2], ip[3]);
  }
  
  scanServerMemory = true;
  scanServerList = false;
  
  scanIP = subnet;
  scanMemoryIPIndex = 0;
}

void stopScanServer() {
  if (serverIP == IPAddress(0,0,0,0)) {
    Serial.println("Server not found");
    setServerMode();
  } else if (serverIP == localIP) {
    // ничего не делать, если мы сервер
  } else {
    Serial.print("Server found: ");
    Serial.println(serverIP);
    isServer = false;

    if (scanServerList) { // Если сервер был найден из полного списка адресов
      Serial.println("Save server list");
      for (int i = SERVERMEM_CNT - 1; i > 0; i--) { // переопределяем память серверов (сдвигаем на один адрес)
        ipMemory[i] = ipMemory[i-1];
      }
      ipMemory[0] = serverIP;

      for (int i = 0; i < SERVERMEM_CNT; i++) {
        for (int j = 0; j < 4; j++) {
          EEPROM.write(SERVERMEM_ADDR + i * 4 + j, ipMemory[i][j]);
        }
      }
      EEPROM.commit();
    }

    HTTPClient http;
    http.begin("http://" + serverIP.toString() + "/info");
    int httpCode = http.GET();
    String json = http.getString();
    http.end();
    
    if (httpCode == HTTP_CODE_OK) {
      JsonObject& root = jsonBuffer.parseObject(json);
      if (root["result"] == "OK") {
        uint8_t serverPerformance = String((const char*)root["server"]).toInt();
        if (performance > serverPerformance) {
          Serial.print("Battle for leadership");
          http.begin("http://" + serverIP.toString() + "/system/changeServer?ip=" + localIP.toString() + "&performance=" + performance);
          int httpCode2 = http.GET();
          String json2 = http.getString();
          http.end();

          if (httpCode2 == HTTP_CODE_OK) {
            JsonObject& root2 = jsonBuffer.parseObject(json2);
            if (root2["result"] == "OK") {
              Serial.println(" - won");
              isServer = true;
              serverIP = localIP;
            } else {
              Serial.println(" - fail");
            }
          } else {
            Serial.println(" - fail");
          }
        }
      }
    }
  }
  
  scanServer = false;
  scanServerMemory = false;
  scanServerList = false;
}

void setServerMode() {
  Serial.println("Set to server mode");
  isServer = true;
  serverIP = localIP;
  stopScanServer();
}

void setServer(IPAddress ip) {
  if (checkIsServer(ip)) {
    serverIP = ip;
    isServer = false;
    stopScanServer();
  }
}

void doScanServer() {
  if (scanServerMemory) { // Scan from memory
    IPAddress memIP = ipMemory[scanMemoryIPIndex];
    if (getSubnet(memIP, netmask) == subnet) {
      if (findServer(memIP)) {
        stopScanServer();
      }
    }
    
    scanMemoryIPIndex++;
    if (scanMemoryIPIndex >= SERVERMEM_CNT) {
      scanServerMemory = false;
      scanServerList = true;
    }
  } else if (scanServerList) { // Scan from full list
    scanIP = nextIP(scanIP);
  
    if (scanIP != broadcastIP) {
      if (scanIP != localIP) {
        if (findServer(scanIP)) {
          stopScanServer();
        }
      }
    } else {
      stopScanServer();
    }
  } else {
    stopScanServer();
  }
}

boolean findServer(IPAddress checkIP) {
  if (checkIP == localIP)
    return false;

  Serial.print("Check:   ");
  Serial.print(checkIP);
  if (!Ping.ping(checkIP, 1)) {
    Serial.println(" Offline");
    return false;
  }

  HTTPClient http;
  http.begin("http://" + checkIP.toString() + "/ping");
  Serial.print(" http ");
  int httpCode = http.GET();
  String json = http.getString();
  http.end();
  Serial.println(httpCode);
  
  if (httpCode == HTTP_CODE_OK) {
    JsonObject& root = jsonBuffer.parseObject(json);
    if (root["result"] == "OK") {
      IPAddress ip = parseIP(String((const char*)root["server"]));
      if (ip != IPAddress(0,0,0,0)) {
        if (checkIsServer(ip)) {
          serverIP = ip;
          return true;
        }
      }
    }
  }

  return false;
}

boolean checkIsServer(IPAddress ip) {
  Serial.print("Check is server: ");
  Serial.print(ip);

//  if (!Ping.ping(ip, 1)) {
//    Serial.println(" Offline");
//    return false;
//  }

  HTTPClient http;
  http.begin("http://" + ip.toString() + "/info");
  Serial.print(" http ");
  
  int httpCode = http.GET();
  String json = http.getString();
  http.end();
  Serial.println(httpCode);
  
  if (httpCode == HTTP_CODE_OK) {
    JsonObject& root = jsonBuffer.parseObject(json);
    if (root["result"] == "OK") {
      return root["isserver"];
    }
  }
  
  return false;
}

void sendHealthcheck() {
  if (localIP == serverIP) {
    Serial.println("Some error was occured. Restart server scan");
    startScanServer();
    return;
  }
  
  Serial.println("Send healthcheck");
  HTTPClient http;
  http.begin("http://" + serverIP.toString() + "/system/healthcheck?mac=" + WiFi.macAddress() + "&ip=" + localIP.toString());
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String json = http.getString();
    JsonObject& root = jsonBuffer.parseObject(json);
    if (root["result"] != "OK") {
      serverIP = parseIP(String((const char*)root["server"]));
      if (serverIP == IPAddress(0,0,0,0))
        startScanServer();
    }
  } else {
    startScanServer();
  }
  http.end();
}

IPAddress findIPByMac(byte mac[6]) {
  for (int i = 0; i < MACTABLE_SIZE; i++) {
    if (macEqual(mactable[i].mac, mac)) {
      return mactable[i].ip;
    }
  }

  return IPAddress(0,0,0,0);
}

void updateMacTable(byte mac[6], IPAddress ip) {
  for (int i = 0; i < MACTABLE_SIZE; i++) {
    if (macEqual(mactable[i].mac, mac)) {
      mactable[i].ip = ip;
      mactable[i].time = millis();
      return;
    }
  }

  for (int i = 0; i < MACTABLE_SIZE; i++) {
    if (macEmpty(mactable[i].mac)) {
      memcpy(mactable[i].mac, mac, 6);
      mactable[i].ip = ip;
      mactable[i].time = millis();
      return;
    }
  }
}

void parseMacAddress(String macStr, byte mac[6]) {  
  for (int i = 0; i < 6; i++) {
    mac[i] = strhex2byte(macStr.substring(i * 3, i * 3 + 2));
  }
}

byte strhex2byte(String hex) {
  if (hex.length() != 2) {
    return 0;
  }
  hex.toUpperCase();
  
  byte result = 0;
  result = ((hex[0] >= '0' && hex[0] <= '9') ? hex[0] - '0' : hex[0] - 'A' + 10) << 4;
  result |= ((hex[1] >= '0' && hex[1] <= '9') ? hex[1] - '0' : hex[1] - 'A' + 10);
  return result;
}

boolean macEqual(byte mac1[6], byte mac2[6]) {
  for (int i = 0; i < 6; i++) {
    if (mac1[i] != mac2[i]) {
      return false;
    }
  }
  return true;
}

boolean macEmpty(byte mac[6]) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0) {
      return false;
    }
  }
  return true;
}

IPAddress getSubnet(IPAddress ip, IPAddress netmask) {
  return IPAddress(ip[0] & netmask[0], ip[1] & netmask[1], ip[2] & netmask[2], ip[3] & netmask[3]);
}

IPAddress getBroadcast(IPAddress subnet, IPAddress netmask) {
  return IPAddress(subnet[0] + ~netmask[0], subnet[1] + ~netmask[1], subnet[2] + ~netmask[2], subnet[3] + ~netmask[3]);
}

IPAddress rotateIP(IPAddress ip) {
  return IPAddress(ip[3], ip[2], ip[1], ip[0]);
}

IPAddress nextIP(IPAddress ip) {
  return rotateIP(rotateIP(ip) + 1);
}

IPAddress parseIP(String ip) {
  byte b0 = ip.substring(0, ip.indexOf(".")).toInt();
  ip = ip.substring(ip.indexOf(".") + 1);
  byte b1 = ip.substring(0, ip.indexOf(".")).toInt();
  ip = ip.substring(ip.indexOf(".") + 1);
  byte b2 = ip.substring(0, ip.indexOf(".")).toInt();
  ip = ip.substring(ip.indexOf(".") + 1);
  byte b3 = ip.toInt();
  return IPAddress(b0, b1, b2, b3);
}
