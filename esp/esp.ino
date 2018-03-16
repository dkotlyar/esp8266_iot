#include "core.h"
#include "wificore.h"

auto LED_PIN = 4;

void setup() {
  coreSetup();
  wifiSetup();

  registerHandleSerialKeyValue(serialKV);
  registerHandleSerialCmd(serialCmd);
  
  createRegister("led", BOOL, READWRITE);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, HIGH);
  delay(300);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  coreLoop();
  wifiLoop();

  register_t* ledReg = getRegister("led");
  digitalWrite(LED_PIN, ledReg->boolValue);
}

void serialKV(String k, String v) {
  Serial.print("Submited ");
  Serial.print(k);
  Serial.print("=");
  Serial.println(v);
}

void serialCmd(String cmd) {
  Serial.print("Command ");
  Serial.println(cmd);
}

