#include "core.h"

auto LED_PIN = 4;

void setup() {
  coreSetup();

  registerHandleSerialKeyValue(serialKV);
  registerHandleSerialCmd(serialCmd);
  
  createRegister("led", BOOL);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  coreLoop();

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

