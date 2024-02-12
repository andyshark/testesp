#include <EEPROM.h>
#include "Arduino.h"

// чтение
unsigned long EEPROM_ulong_read(int addr) {    
  byte raw[4];
  for(byte i = 0; i < 4; i++) raw[i] = EEPROM.read(addr+i);
  unsigned long &num = (unsigned long&)raw;
  return num;
}

// запись
void EEPROM_ulong_write(int addr, unsigned long num) {
  byte raw[4];
  (unsigned long&)raw = num;
  for(byte i = 0; i < 4; i++) EEPROM.write(addr+i, raw[i]);
  EEPROM.commit();
}

// чтение
long EEPROM_long_read(int addr) {    
  byte raw[4];
  for(byte i = 0; i < 4; i++) raw[i] = EEPROM.read(addr+i);
  long &num = (long&)raw;
  return num;
}

// запись
void EEPROM_long_write(int addr, long num) {
  byte raw[4];
  (long&)raw = num;
  for(byte i = 0; i < 4; i++) EEPROM.write(addr+i, raw[i]);
  EEPROM.commit();
}

// чтение
unsigned int EEPROM_uint_read(int addr) {    
  byte raw[2];
  for(byte i = 0; i < 2; i++) raw[i] = EEPROM.read(addr+i);
  unsigned int &num = (unsigned int&)raw;
  return num;
}
// запись
void EEPROM_uint_write(int addr, unsigned int num) {
  byte raw[2];
  (unsigned int&)raw = num;
  for(byte i = 0; i < 2; i++) EEPROM.write(addr+i, raw[i]);
  EEPROM.commit();
}

// чтение
int16_t EEPROM_int_read(int addr) {    
  byte raw[2];
  for(byte i = 0; i < 2; i++) raw[i] = EEPROM.read(addr+i);
  int16_t &num = (int16_t&)raw;
  return num;
}

// запись
void EEPROM_int_write(int addr, int16_t num) {
  byte raw[2];
  (int&)raw = num;
  for(byte i = 0; i < 2; i++) EEPROM.write(addr+i, raw[i]);
  EEPROM.commit();
}

// чтение
float EEPROM_float_read(int addr) {    
  byte raw[4];
  for(byte i = 0; i < 4; i++) raw[i] = EEPROM.read(addr+i);
  float &num = (float&)raw;
  return num;
}

// запись
void EEPROM_float_write(int addr, float num) {
  byte raw[4];
  (float&)raw = num;
  for(byte i = 0; i < 4; i++) EEPROM.write(addr+i, raw[i]);
  EEPROM.commit();
}

// чтение
uint64_t EEPROM_uint64_read(int addr) {    
  byte raw[8];
  for(byte i = 0; i < 8; i++) raw[i] = EEPROM.read(addr+i);
  uint64_t &num = (uint64_t&)raw;
  return num;
}

// запись
void EEPROM_uint64_write(int addr, uint64_t num) {
  byte raw[8];
  (uint64_t&)raw = num;
  for(byte i = 0; i < 8; i++) EEPROM.write(addr+i, raw[i]);
  EEPROM.commit();
}

// чтение
void EEPROM_CharArr_read(int addr, char* cptr, uint8_t num) {    
  for(byte i = 0; i < 8; i++) {
    *cptr = EEPROM.read(addr+i);
    cptr++;
  }
}

// запись
void EEPROM_CharArr_write(int addr, char* cptr, uint8_t num) {
  for(byte i = 0; i < 8; i++) {
    EEPROM.write(addr+i, *cptr);
    cptr++;
  }  
  EEPROM.commit();
}

// чтение (первый байт это длинна строки)
void EEPROM_String_read(int addr, char* buffer) {
  int newStrLen = EEPROM.read(addr);
  if (newStrLen >= sizeof(buffer)) {
    newStrLen = sizeof(buffer) - 1;  // Уменьшаем длину строки на 1, чтобы оставить место для нулевого символа
  }
  for (int i = 0; i < newStrLen; i++) {
    buffer[i] = EEPROM.read(addr + 1 + i);
  }
}

// запись (первый байт это длинна строки)
void EEPROM_String_write(int addr, char* buffer) {
  byte len = strlen(buffer);
  if (len > sizeof(buffer) - 1) len = sizeof(buffer) - 1;  // Исправлено для учета размера буфера
  EEPROM.write(addr, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + 1 + i, buffer[i]);
  }
  EEPROM.commit();
}

//crc
unsigned long eeprom_onA4_crc(void) {
  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };
 
  unsigned long crc = ~0L;
 
  for (uint16_t index = 4 ; index < EEPROM.length()  ; ++index) {
    crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}
