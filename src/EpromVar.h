#ifndef __EPROMVAR
#define __EPROMVAR
#include <Arduino.h>
#include <EEPROM.h>

extern unsigned long EEPROM_ulong_read(int addr);
extern long EEPROM_long_read(int addr);
extern unsigned int EEPROM_uint_read(int addr);
extern int16_t EEPROM_int_read(int addr);
extern float EEPROM_float_read(int addr);
extern uint64_t EEPROM_uint64_read(int addr);
extern void EEPROM_ulong_write(int addr, unsigned long num);
extern void EEPROM_long_write(int addr, long num);
extern void EEPROM_uint_write(int addr, unsigned int num);
extern void EEPROM_int_write(int addr, int16_t num);
extern void EEPROM_float_write(int addr, float num);
extern void EEPROM_uint64_write(int addr, uint64_t num);
extern unsigned long eeprom_onA4_crc(void);
extern void EEPROM_CharArr_read(int addr, char *cptr, uint8_t num);
extern void EEPROM_String_write(int addr, char* buffer);
extern void EEPROM_String_write(int addr, const String &strToWrite);
extern String EEPROM_String_read(int addr, char* buffer);
#endif
