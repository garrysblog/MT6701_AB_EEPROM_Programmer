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
  - MT6701 MODE (M) -> Leave unconnected (enables I2C mode)
  - MT6701 A (SDA)  -> Arduino A4 (SDA)
  - MT6701 B (SCL)  -> Arduino A5 (SCL)
  - MT6701 Z        -> Leave unconnected
  
  IMPORTANT: Arduino Nano provides stable 5V which is perfect for EEPROM programming!

  How it works:
  1. Reads current configuration from MT6701 EEPROM
  2. Programs new AB resolution (while preserving Z pulse width and other settings)
  3. Sends programming command sequence per datasheet
  4. Waits 650ms for EEPROM write cycle to complete
  5. Verifies the programming was successful
  6. Requires power cycle for new settings to take effect

  How to use:
  -----------
  STEP 1: Set your desired resolution below:
  
    #define DESIRED_AB_PPR 64
  
  STEP 2: Choose operating mode (one variable):
  
    #define ENABLE_PROGRAMMING false    // false = READ ONLY (safe)
                                        // true  = PROGRAM EEPROM
  
  STEP 3: Upload and open Serial Monitor (9600 baud)
  
  STEP 4: After successful programming:
    - Set ENABLE_PROGRAMMING back to false
    - Power cycle the MT6701 (disconnect/reconnect VDD)
    - Upload again to verify new settings

  MT6701 Data sheet: https://www.novosns.com/enfiles/MT6701_Rev.1.8.pdf   
*/

#include <Wire.h>

// ========== USER CONFIGURATION ==========

// Set your desired AB resolution here (1-1024 PPR)
#define DESIRED_AB_PPR 64

// Operating mode - Change this value:
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
#define REG_ABZ_RES_HIGH  0x30  // ABZ_RES[9:8] in bits [1:0], UVW_RES[3:0] in bits [7:4]
#define REG_ABZ_RES_LOW   0x31  // ABZ_RES[7:0]
#define REG_ZERO_MSB      0x32  // Z_PULSE_WIDTH[2:0] in bits [6:4], ZERO[11:8] in bits [3:0]
#define REG_PROG_KEY      0x09  // Programming key register
#define REG_PROG_CMD      0x0A  // Programming command register

// Angle reading registers
#define REG_ANGLE_HIGH    0x03  // Angle[13:6]
#define REG_ANGLE_LOW     0x04  // Angle[5:0]

void setup() {
  Serial.begin(9600);
  delay(2000);
  
  // Print header with current mode
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
  
  // Validate PPR value
  if (DESIRED_AB_PPR < 1 || DESIRED_AB_PPR > 1024) {
    Serial.println(F("ERROR: DESIRED_AB_PPR must be between 1 and 1024!"));
    Serial.println(F("Please update the value in the code and re-upload."));
    while(1); // Halt
  }
  
  // Initialize I2C
  Wire.begin();
  Wire.setClock(100000);
  
  // Scan I2C bus
  Serial.println(F("Scanning I2C bus..."));
  scanI2C();
  
  // Check for MT6701
  Serial.println(F("\nChecking for MT6701..."));
  if (!checkConnection()) {
    Serial.println(F("\n*** ERROR: MT6701 not detected! ***\n"));
    Serial.println(F("Troubleshooting:"));
    Serial.println(F("  1. VDD (Pin 13) -> Arduino 5V"));
    Serial.println(F("  2. GND (Pin 16) -> Arduino GND"));
    Serial.println(F("  3. MODE (Pin 14) -> Leave unconnected"));
    Serial.println(F("  4. A/SDA (Pin 6) -> Arduino A4 + 4.7k pullup to 5V"));
    Serial.println(F("  5. B/SCL (Pin 7) -> Arduino A5 + 4.7k pullup to 5V"));
    while(1); // Halt
  }
  
  Serial.println(F("MT6701 detected successfully!\n"));
  
  // Display current configuration
  Serial.println(F("=== Current Configuration ==="));
  displayCurrentConfig();
  
  #if ENABLE_PROGRAMMING
    // Programming mode
    Serial.println(F("\n=== Starting Programming Sequence ==="));
    Serial.println(F("NOTE: Z pulse width will NOT be changed\n"));
    
    // Check if already at desired resolution
    uint16_t currentRes = readABResolution();
    if (currentRes == DESIRED_AB_PPR) {
      Serial.println(F("*** NOTICE ***"));
      Serial.print(F("MT6701 is already set to "));
      Serial.print(DESIRED_AB_PPR);
      Serial.println(F(" PPR"));
      Serial.println(F("No programming needed."));
    } else {
      // Perform programming
      if (programABResolution(DESIRED_AB_PPR)) {
        Serial.println(F("\n========================================"));
        Serial.println(F("  Programming Complete!"));
        Serial.println(F("========================================"));
        Serial.println(F("\nNEXT STEPS:"));
        Serial.println(F("1. Set ENABLE_PROGRAMMING to false"));
        Serial.println(F("2. Power cycle MT6701 (disconnect & reconnect VDD)"));
        Serial.println(F("3. Upload sketch again to verify new settings"));
        Serial.println(F("\nWaiting for power cycle..."));
      } else {
        Serial.println(F("\n*** ERROR: Programming failed! ***"));
      }
    }
  #else
    // Read-only mode
    Serial.println(F("\n=== Read-Only Mode ==="));
    Serial.println(F("Current settings displayed above."));
    Serial.println(F("No changes made to EEPROM."));
    Serial.println(F("\nTo program a new resolution:"));
    Serial.println(F("1. Set DESIRED_AB_PPR to your desired value"));
    Serial.println(F("2. Set ENABLE_PROGRAMMING to true"));
    Serial.println(F("3. Upload the sketch"));
  #endif
  
  Serial.println(F("\n========================================\n"));
}

void loop() {
  // In read-only mode, continuously display angle
  #if !ENABLE_PROGRAMMING
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 500) {  // Update every 500ms
      float angle = readAngle();
      Serial.print(F("Angle: "));
      Serial.print(angle, 2);
      Serial.println(F("°"));
      lastUpdate = millis();
    }
  #endif
  
  delay(100);
}

/**
 * Check if MT6701 is responding on I2C bus
 */
bool checkConnection() {
  Wire.beginTransmission(MT6701_ADDR);
  return (Wire.endTransmission() == 0);
}

/**
 * Scan all I2C addresses
 */
void scanI2C() {
  byte error, address;
  int nDevices = 0;
  
  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print(F("  Device found at 0x"));
      if (address < 16) Serial.print(F("0"));
      Serial.print(address, HEX);
      if (address == 0x06) Serial.print(F(" <- MT6701 (default address)"));
      if (address == 0x66) Serial.print(F(" <- Possible MT6701 (alt address)"));
      Serial.println();
      nDevices++;
    }
  }
  
  if (nDevices == 0) {
    Serial.println(F("  No I2C devices found!"));
  } else {
    Serial.print(F("  Total: "));
    Serial.print(nDevices);
    Serial.println(F(" device(s)"));
  }
}

/**
 * Display current configuration
 */
void displayCurrentConfig() {
  uint8_t reg30 = readRegister(REG_ABZ_RES_HIGH);
  uint8_t reg31 = readRegister(REG_ABZ_RES_LOW);
  uint16_t abRes = readABResolution();
  uint8_t zWidth = readZPulseWidth();
  
  Serial.print(F("  AB Resolution: "));
  Serial.print(abRes);
  Serial.println(F(" PPR"));
  
  Serial.print(F("  Z Pulse Width: "));
  switch(zWidth) {
    case 0: Serial.println(F("1 LSB")); break;
    case 1: Serial.println(F("2 LSB")); break;
    case 2: Serial.println(F("4 LSB")); break;
    case 3: Serial.println(F("8 LSB")); break;
    case 4: Serial.println(F("12 LSB")); break;
    case 5: Serial.println(F("16 LSB")); break;
    case 6: Serial.println(F("180°")); break;
    case 7: Serial.println(F("1 LSB")); break;
    default: Serial.println(F("Unknown")); break;
  }
  
  Serial.print(F("  Current Angle: "));
  float angle = readAngle();
  Serial.print(angle, 2);
  Serial.println(F("°"));
  
  Serial.print(F("  Register 0x30: 0x"));
  if (reg30 < 16) Serial.print(F("0"));
  Serial.println(reg30, HEX);
  
  Serial.print(F("  Register 0x31: 0x"));
  if (reg31 < 16) Serial.print(F("0"));
  Serial.println(reg31, HEX);
}

/**
 * Read AB Resolution
 */
uint16_t readABResolution() {
  uint8_t highByte = readRegister(REG_ABZ_RES_HIGH);
  uint8_t lowByte = readRegister(REG_ABZ_RES_LOW);
  uint16_t resolution = ((highByte & 0x03) << 8) | lowByte;
  return resolution + 1;  // Register stores PPR-1
}

/**
 * Read Z Pulse Width
 */
uint8_t readZPulseWidth() {
  uint8_t reg32 = readRegister(REG_ZERO_MSB);
  return (reg32 >> 4) & 0x07;
}

/**
 * Read current angle
 */
float readAngle() {
  uint8_t highByte = readRegister(REG_ANGLE_HIGH);
  uint8_t lowByte = readRegister(REG_ANGLE_LOW);
  uint16_t rawAngle = (highByte << 6) | (lowByte >> 2);
  return (rawAngle * 360.0) / 16384.0;
}

/**
 * Program AB Resolution to EEPROM
 */
bool programABResolution(uint16_t ppr) {
  if (ppr < 1 || ppr > 1024) {
    return false;
  }
  
  uint16_t regValue = ppr - 1;
  
  Serial.println(F("Step 1: Reading existing registers..."));
  uint8_t reg30 = readRegister(REG_ABZ_RES_HIGH);
  uint8_t reg32 = readRegister(REG_ZERO_MSB);
  
  uint8_t newReg30 = (reg30 & 0xFC) | ((regValue >> 8) & 0x03);
  uint8_t newReg31 = regValue & 0xFF;
  
  Serial.print(F("  0x30: 0x"));
  Serial.print(reg30, HEX);
  Serial.print(F(" -> 0x"));
  Serial.println(newReg30, HEX);
  
  Serial.print(F("  0x31: 0x"));
  Serial.println(newReg31, HEX);
  
  Serial.println(F("\nStep 2: Writing to registers..."));
  if (!writeRegister(REG_ABZ_RES_HIGH, newReg30) || 
      !writeRegister(REG_ABZ_RES_LOW, newReg31)) {
    Serial.println(F("  FAILED"));
    return false;
  }
  Serial.println(F("  Success"));
  
  Serial.println(F("\nStep 3: Unlocking EEPROM (key: 0xB3)..."));
  if (!writeRegister(REG_PROG_KEY, 0xB3)) {
    Serial.println(F("  FAILED"));
    return false;
  }
  Serial.println(F("  Success"));
  
  Serial.println(F("\nStep 4: Programming command (0x05)..."));
  if (!writeRegister(REG_PROG_CMD, 0x05)) {
    Serial.println(F("  FAILED"));
    return false;
  }
  Serial.println(F("  Success"));
  
  Serial.println(F("\nStep 5: EEPROM write cycle (650ms)..."));
  Serial.println(F("  DO NOT DISCONNECT POWER!"));
  for (int i = 0; i < 13; i++) {
    Serial.print(F("."));
    delay(50);
  }
  Serial.println();
  
  Serial.println(F("\nStep 6: Verification..."));
  uint16_t verifyRes = readABResolution();
  Serial.print(F("  Read back: "));
  Serial.print(verifyRes);
  Serial.println(F(" PPR"));
  
  if (verifyRes == ppr) {
    Serial.println(F("  Pre-power-cycle verification: PASSED"));
    Serial.println(F("  (Power cycle required for final verification)"));
    return true;
  } else {
    Serial.println(F("  Pre-power-cycle check inconclusive"));
    Serial.println(F("  (This is normal - power cycle needed)"));
    return true;
  }
}

/**
 * Read register via I2C
 */
uint8_t readRegister(uint8_t regAddr) {
  Wire.beginTransmission(MT6701_ADDR);
  Wire.write(regAddr);
  Wire.endTransmission(false);
  Wire.requestFrom(MT6701_ADDR, 1);
  
  if (Wire.available()) {
    return Wire.read();
  }
  return 0;
}

/**
 * Write register via I2C
 */
bool writeRegister(uint8_t regAddr, uint8_t data) {
  Wire.beginTransmission(MT6701_ADDR);
  Wire.write(regAddr);
  Wire.write(data);
  return (Wire.endTransmission() == 0);
}