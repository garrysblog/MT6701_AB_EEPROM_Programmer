/*
  MT6701QT EEPROM Programmer for Arduino Nano (5V)

  Generated with the assistance of Claude AI
 
  This program is free software: you can redistribute it and/or modify it under the 
  terms of the MIT License https://opensource.org/license/mit
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND
  
  This sketch programs the AB Resolution in the MT6701's EEPROM
  The Z pulse width is NOT changed - it retains its current value

  Hardware Connections:
  - MT6701 VDD      -> Arduino 5V
  - MT6701 GND      -> Arduino GND
  - MT6701 A        -> Arduino A4 (used for both I2C SDA and encoder A)
  - MT6701 B        -> Arduino A5 (used for both I2C SCL and encoder B)
  - MT6701 Z        -> Leave unconnected
    
  IMPORTANT: Arduino Nano provides stable 5V which is perfect for EEPROM programming!

  How it works:
  1. Reads current configuration from MT6701 EEPROM
  2. Programs new AB resolution (while preserving Z pulse width and other settings)
  3. Sends programming command sequence per datasheet
  4. Waits 650ms for EEPROM write cycle to complete
  5. Verifies the programming was successful
  6. Requires power cycle to test new setting has been stored

  How to use:
  -----------
  STEP 1: Set your desired resolution below:
  
    #define DESIRED_AB_PPR 64
  
  STEP 2: Choose operating mode:
  
    #define ENABLE_PROGRAMMING false    // false = READ ONLY (safe)
                                        // true  = PROGRAM EEPROM
  
  STEP 3: Upload and open Serial Monitor (9600 baud)
  
  STEP 4: After successful programming:
    - Set ENABLE_PROGRAMMING back to false
    - Power cycle the MT6701 (disconnect/reconnect VDD)
    - Set ENABLE_PROGRAMMING false and upload again to verify new settings

  MT6701 Data sheet: https://www.novosns.com/enfiles/MT6701_Rev.1.8.pdf   
*/

#include <Wire.h>

// ========== USER CONFIGURATION ==========

// Set your desired AB resolution here (1-1024 PPR)
#define DESIRED_AB_PPR 64

// Operating mode:
//   false = READ ONLY (safe - just displays current settings)
//   true  = PROGRAM EEPROM (CAUTION - writes to EEPROM)
#define ENABLE_PROGRAMMING false

// ========================================

// Compile-time safety check
#if ENABLE_PROGRAMMING
  #warning "WARNING: EEPROM PROGRAMMING IS ENABLED! Set ENABLE_PROGRAMMING to false after programming."
#endif

// MT6701 I2C Address (7-bit)
#define MT6701_ADDR 0x06

// EEPROM Register Addresses
#define REG_ABZ_RES_HIGH  0x30
#define REG_ABZ_RES_LOW   0x31
#define REG_ZERO_MSB      0x32
#define REG_PROG_KEY      0x09
#define REG_PROG_CMD      0x0A

void setup() {
  Serial.begin(9600);
  delay(2000);
  
  Serial.println(F("========================================"));
  Serial.println(F("  MT6701 EEPROM Programmer"));
  Serial.println(F("  Arduino Nano (5V) Version"));
  Serial.println(F("========================================"));
  
#if ENABLE_PROGRAMMING
  Serial.println(F("\nMODE: PROGRAM EEPROM"));
  Serial.println(F("*** WARNING: EEPROM PROGRAMMING ENABLED ***"));
  Serial.print(F("Target AB Resolution: "));
  Serial.print(DESIRED_AB_PPR);
  Serial.println(F(" PPR"));
#else
  Serial.println(F("\nMODE: READ ONLY (Safe - No changes)"));
#endif
  
  Serial.println(F("========================================\n"));
  
  if (DESIRED_AB_PPR < 1 || DESIRED_AB_PPR > 1024) {
    Serial.println(F("ERROR: DESIRED_AB_PPR must be between 1 and 1024!"));
    while (1);
  }
  
  Wire.begin();
  Wire.setClock(100000);
  
  Serial.println(F("Scanning I2C bus..."));
  scanI2C();
  
  Serial.println(F("\nChecking for MT6701..."));
  if (!checkConnection()) {
    Serial.println(F("\n*** ERROR: MT6701 not detected! ***\n"));
    Serial.println(F("Troubleshooting:"));
    Serial.println(F("  1. VDD -> Arduino 5V"));
    Serial.println(F("  2. GND -> Arduino GND"));
    Serial.println(F("  3. MODE -> Leave unconnected"));
    Serial.println(F("  4. SDA -> Arduino A4"));
    Serial.println(F("  5. SCL -> Arduino A5"));
    while (1);
  }
  
  Serial.println(F("MT6701 detected successfully!\n"));
  
  Serial.println(F("=== Current Configuration ==="));
  displayCurrentConfig();
  
#if ENABLE_PROGRAMMING
  Serial.println(F("\n=== Starting Programming Sequence ==="));
  
  uint16_t currentRes = readABResolution();
  if (currentRes == DESIRED_AB_PPR) {
    Serial.println(F("Already programmed to desired resolution."));
  } else {
    if (programABResolution(DESIRED_AB_PPR)) {
      Serial.println(F("\nProgramming complete."));
      Serial.println(F("Power cycle required for final verification."));
    } else {
      Serial.println(F("\n*** ERROR: Programming failed! ***"));
    }
  }
#else
  Serial.println(F("\nRead-only mode. No EEPROM changes made."));
#endif
  
  Serial.println(F("\n========================================\n"));
}

void loop() {
  // Nothing to do
  delay(500);
}

/* ---------- Helper Functions ---------- */

bool checkConnection() {
  Wire.beginTransmission(MT6701_ADDR);
  return (Wire.endTransmission() == 0);
}

void scanI2C() {
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  Device found at 0x"));
      if (addr < 16) Serial.print(F("0"));
      Serial.println(addr, HEX);
    }
  }
}

void displayCurrentConfig() {
  uint8_t reg30 = readRegister(REG_ABZ_RES_HIGH);
  uint8_t reg31 = readRegister(REG_ABZ_RES_LOW);
  uint16_t abRes = readABResolution();
  uint8_t zWidth = readZPulseWidth();
  
  Serial.print(F("  AB Resolution: "));
  Serial.print(abRes);
  Serial.println(F(" PPR"));
  
  Serial.print(F("  Z Pulse Width: "));
  switch (zWidth) {
    case 0: Serial.println(F("1 LSB")); break;
    case 1: Serial.println(F("2 LSB")); break;
    case 2: Serial.println(F("4 LSB")); break;
    case 3: Serial.println(F("8 LSB")); break;
    case 4: Serial.println(F("12 LSB")); break;
    case 5: Serial.println(F("16 LSB")); break;
    case 6: Serial.println(F("180°")); break;
    case 7: Serial.println(F("1 LSB (reserved)")); break;
    default: Serial.println(F("Unknown")); break;
  }
  
  Serial.print(F("  Register 0x30: 0x"));
  Serial.println(reg30, HEX);
  
  Serial.print(F("  Register 0x31: 0x"));
  Serial.println(reg31, HEX);
}

uint16_t readABResolution() {
  uint8_t highByte = readRegister(REG_ABZ_RES_HIGH);
  uint8_t lowByte  = readRegister(REG_ABZ_RES_LOW);
  return (((highByte & 0x03) << 8) | lowByte) + 1;
}

uint8_t readZPulseWidth() {
  uint8_t reg32 = readRegister(REG_ZERO_MSB);
  return (reg32 >> 4) & 0x07;
}

bool programABResolution(uint16_t ppr) {
  uint16_t regValue = ppr - 1;
  
  uint8_t reg30 = readRegister(REG_ABZ_RES_HIGH);
  uint8_t newReg30 = (reg30 & 0xFC) | ((regValue >> 8) & 0x03);
  uint8_t newReg31 = regValue & 0xFF;
  
  if (!writeRegister(REG_ABZ_RES_HIGH, newReg30)) return false;
  if (!writeRegister(REG_ABZ_RES_LOW, newReg31))  return false;
  if (!writeRegister(REG_PROG_KEY, 0xB3))         return false;
  if (!writeRegister(REG_PROG_CMD, 0x05))         return false;
  
  delay(650);
  return true;
}

uint8_t readRegister(uint8_t regAddr) {
  Wire.beginTransmission(MT6701_ADDR);
  Wire.write(regAddr);
  Wire.endTransmission(false);
  Wire.requestFrom(MT6701_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

bool writeRegister(uint8_t regAddr, uint8_t data) {
  Wire.beginTransmission(MT6701_ADDR);
  Wire.write(regAddr);
  Wire.write(data);
  return (Wire.endTransmission() == 0);
}
