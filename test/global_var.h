/* global.h */
//-------------------------------------------------------
// глобальные переменные
//-------------------------------------------------------
#ifndef __GLOBAL_VARS
#define __GLOBAL_VARS

#include <Arduino.h>

#define bit_is_clear(var,bit) ((var & (1<<(bit))) ==0)
#define bit_is_set(var,bit) ((var & (1<<(bit))) !=0)  

// П И Н Ы
#define ICDDR  DDRB   //!!!!!!!!!!!!!!!!!
#define ICBIT  PB0    //!!!!!!!!!!!!!!!!!

#if defined(ARDUINO_BLUEPILL_F103C8TXALI)
//#define LED_BUILTIN   2                 // светодиод 
//#define SS1           XX                // PA4, PA15-(5V)
#define SCLK1         10                // PA5, PB3-(5V)
#define MISO1         11                // PA6, PB4-(5V)
#define MOSI1         12                // PA7, PB5
#define RF24IRQ       13                // 
#define RF24CEpin     14                // 
#define VSUP          15                //  
#define TISP          16                // 
#define LEDGND        21                // 
#define SYSLED        22                // 
#define SNSGND        23                //
#define DWLINE        24                //
#define SCLK2         26                // PB13
#define MISO2         27                // PB14
#define MOSI2         28                // PB15

//#define                9                // 
//#define               24                // 
//#define               25                // 
//#define               26                // 
//#define               27                // 
//#define               33                // 
//#define               36                // 
//#define               37                // 
//#define AnPin1        A0                // аналоговый вход 1      5
//#define AnPin2        A1                // аналоговый вход 2      6
#endif

#if defined(ARDUINO_BLUEPILL_F103C6TXALI)
//#define LED_BUILTIN   2                 // светодиод 
//#define SS1           XX                // PA4, PA15-(5V)
#define SCLK1         10                // PA5, PB3-(5V)
#define MISO1         11                // PA6, PB4-(5V)
#define MOSI1         12                // PA7, PB5
#define RF24IRQ       13                // 
#define RF24CEpin     14                // 
#define VSUP          15                //  
#define TISP          16                // 
#define LEDGND        21                // 
#define SYSLED        22                // 
#define SNSGND        23                //
#define DWLINE        24                //
#define SCLK2         26                // PB13
#define MISO2         27                // PB14
#define MOSI2         28                // PB15

//#define                9                // 
//#define               24                // 
//#define               25                // 
//#define               33                // 
//#define               36                // 
//#define               37                // 
//#define AnPin1        A0                // аналоговый вход 1      5
//#define AnPin2        A1                // аналоговый вход 2      6
#endif

                                     
#endif
