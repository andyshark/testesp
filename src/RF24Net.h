#ifndef __RF24NET
#define __RF24NET

#include "global_var.h"                 // подключение глобальных переменных
#include "RF24.h"    // подключение библиотеки NRF24L01
#include <time.h>      
#include <sys/time.h>             

extern void SetupRadio(void);
extern void Store(void);
extern void whyIrq(void);
extern void SendSensorParam(uint8_t nTube, uint8_t AddrWho);
extern void SendPacket(uint8_t nTube);
extern RF24 radio;
#endif
