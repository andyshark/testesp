/* main.cpp */
#define VERSION "1.3.10"

#ifndef STMVERSION
#define STMVERSION T6
#endif

#ifndef INITIALBPS 
#define INITIALBPS 115200UL // initial expected communication speed with the host
                            // 230400, 57600, 38400, 19200, 9600 are alternatives
#endif

#include <Arduino.h>
#include <stddef.h>
#include <stdio.h>
//#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <avr/pgmspace.h>
#include "global_var.h"        // подключение экспорта глобальных переменных  
#include<SPI.h>

// некоторые ограничения по размеру
#define MAXBUF        150      // входной буфер для связи GDB
#define MAXMEMBUF     150      // размер буфера памяти
#define MAXPAGESIZE   256      // максимальное количество байт на одной странице флэш-памяти (для микроконтроллеров 64 КБ)
#define MAXBREAK       33      // максимальное количество активных точек останова (нам нужно вдвое больше записей для отложенной установки/удаления точек останова!)

// скорость передачи данных 
#define SPEEDHIGH     275000UL // ограничение максимальной скорости передачи данных для DW
#define SPEEDLOW      137000UL // нормальный предел скорости

// количество допустимых таймаутов для одной команды DW
#define TIMEOUTMAX 20

// сигналы
#define SIGHUP  1     // связь с целью потеряна
#define SIGINT  2     // Interrupt  - пользователь прервал программу (UART ISR)
#define SIGILL  4     // неизвестная инструкция
#define SIGTRAP 5     // Ловушка трассировки — остановлен на точке останова
#define SIGABRT 6     // Прерывание из-за фатальной ошибки

// виды фатальных ошибок
#define NO_FATAL 0
#define CONNERR_NO_ISP_OR_DW_REPLY 1                        // ошибка соединения: нет ответа от ISP или DW
#define CONNERR_UNSUPPORTED_MCU 2                           // ошибка подключения: Микроконтроллер не поддерживается
#define CONNERR_LOCK_BITS 3                                 // ошибка соединения: установлены биты блокировки
#define CONNERR_STUCKAT1_PC 4                               // ошибка подключения: у Микроконтроллера есть застрявшие биты в программном счетчике
#define CONNERR_UNKNOWN 5                                   // неизвестная ошибка соединения
#define NO_FREE_SLOT_FATAL 101                              // нет свободного места в структуре брейкпоинтов
#define PACKET_LEN_FATAL 102                                // длина пакета слишком велика
#define WRONG_MEM_FATAL 103                                 // неправильный тип памяти
#define NEG_SIZE_FATAL 104                                  // отрицательный размер буфера
#define RESET_FAILED_FATAL 105                              // сброс не удался
#define READ_PAGE_ADDR_FATAL 106                            // адрес, который не указывает на начало страницы в операции чтения
#define FLASH_READ_FATAL 107                                // ошибка при чтении с флэш-памяти
#define SRAM_READ_FATAL 108                                 // ошибка при чтении из оперативной памяти
#define WRITE_PAGE_ADDR_FATAL 109                           // неверный адрес страницы при написании
#define FLASH_ERASE_FATAL 110                               // ошибка при стирании флеш памяти
#define NO_LOAD_FLASH_FATAL 111                             // ошибка при загрузке страницы во флэш-буфер
#define FLASH_PROGRAM_FATAL 112                             // ошибка при программировании флеш-страницы
#define HWBP_ASSIGNMENT_INCONSISTENT_FATAL 113              // HWBP назначение не корректно
#define SELF_BLOCKING_FATAL 114                             // в коде не должно быть инструкции BREAK
#define FLASH_READ_WRONG_ADDR_FATAL 115                     // пытаюсь прочитать флеш-слово по нечетному адресу
#define NO_STEP_FATAL 116                                   // не удалось выполнить одношаговую операцию
#define RELEVANT_BP_NOT_PRESENT 117                         // идентифицированный соответствующий BP больше не присутствует 
#define INPUT_OVERLFOW_FATAL 118                            // Переполнение входного буфера - вообще не должно произойти!
#define WRONG_FUSE_SPEC_FATAL 119                           // Спецификацию fuse не возможно изменить
#define BREAKPOINT_UPDATE_WHILE_FLASH_PROGRAMMING_FATAL 120 // Фатальная ошибка (не должно произойти)!
#define DW_TIMEOUT_FATAL 121                                // таймаут при чтении из строки DW
#define DW_READREG_FATAL 122                                // таймаут при чтении регистра
#define DW_READIOREG_FATAL 123                              // таймаут во время операции чтения регистра
#define REENABLERWW_FATAL 124                               // тайм-аут во время повторной операции RWW
#define EEPROM_READ_FATAL 125                               // таймаут во время чтения EEPROM
#define BAD_INTERRUPT_FATAL 126                             // плохое прерывание

// некоторые маски для интерпретации адресов памяти
#define MEM_SPACE_MASK 0x00FF0000 // маска для определения того, какая область памяти имеется в виду
#define FLASH_OFFSET   0x00000000 // flash адресуется начиная с 0
#define SRAM_OFFSET    0x00800000 // Адрес RAM из GBD равен (реальный адрес + 0x00800000)
#define EEPROM_OFFSET  0x00810000 // Адрес EEPROM из GBD равен (реальный адрес + 0x00810000)

// коды инструкций
#define BREAKCODE 0x9598

// некоторые переменные GDB
struct breakpoint
{
  bool used:1;            // breakpoint используется, т. е. был установлен ранее; будет освобожден, если не активирован перед следующим выполнением
  bool active:1;          // breakpoint активен, т.е. установлен ​​в GDB
  bool inflash:1;         // breakpoint находится во флэш-памяти, т. е. в памяти установлен BREAK instr.
  bool hw:1;              // breakpoint является аппаратной точкой останова, т. е. не устанавливается в памяти, но используется HWBP
  unsigned int waddr;     // word адрес точки останова
  unsigned int opcode;    // opcode который был заменен на BREAK (используется прямой порядок следования байтов)
} bp[MAXBREAK*2];

byte bpcnt;                 // количество АКТИВНЫХ точек останова (их может быть столько, сколько MAXBREAK использованных с последнего выполнения!)
byte bpused;                // количество ИСПОЛЬЗУЕМЫХ точек останова, которые могут быть не все активны
byte maxbreak = MAXBREAK;   // фактическое количество разрешенных активных точек останова

unsigned int hwbp = 0xFFFF; // одна аппаратная точка останова (word address hardware breakpoint)

enum statetype {NOTCONN_STATE, PWRCYC_STATE, ERROR_STATE, CONN_STATE, LOAD_STATE, RUN_STATE};

struct context {
  unsigned int wpc;       // программный счетчик (адрес в формате word)
  unsigned int sp;        // указатель стека
  byte sreg;              // регистр статуса
  byte regs[32];          // регистры общего назначения
  bool saved:1;           // все регистры сохранены
  statetype state:3;      // состояние системы
  unsigned long bps;      // скорость связи debugWIRE
  unsigned long hostbps;  // скорость связи с хостом
  bool safestep;          // если true, то один шаг безопасным способом, т.е. не прерываемым
} ctx;

// используйте светодиод для сигнализации состояния системы
// Светодиод не горит = не подключен к целевой системе
// Светодиод мигает каждую секунду = цель выключения и включения питания для включения debugWIRE
// Светодиод мигает каждые 1/10 секунды = не удалось подключиться к целевой плате.
// Светодиод горит постоянно = подключен к цели и цель остановлена
// Светодиод горит, но каждую секунду гаснет на 1/10 секунды = код загрузки
// Светодиод мигает каждые 1/3 секунды = мишень движется.
const unsigned int ontimes[6] =  {0,  100, 150, 1, 1, 1};
const unsigned int offtimes[6] = {1, 1000, 150, 0, 0, 0};
volatile unsigned int ontime;     // количество мс вкл.
volatile unsigned int offtime;    // количество мс выключения

// список поддерживаемых микроконтроллеров
const char attiny13[] PROGMEM = "ATtiny13";
const char attiny43[] PROGMEM = "ATtiny43U";
const char attiny2313[] PROGMEM = "ATtiny2313";
const char attiny4313[] PROGMEM = "ATtiny4313";
const char attiny24[] PROGMEM = "ATtiny24";
const char attiny44[] PROGMEM = "ATtiny44";
const char attiny84[] PROGMEM = "ATtiny84";
const char attiny441[] PROGMEM = "ATtiny441";
const char attiny841[] PROGMEM = "ATtiny841";
const char attiny25[] PROGMEM = "ATtiny25";
const char attiny45[] PROGMEM = "ATtiny45";
const char attiny85[] PROGMEM = "ATtiny85";
const char attiny261[] PROGMEM = "ATtiny261";
const char attiny461[] PROGMEM = "ATtiny461";
const char attiny861[] PROGMEM = "ATtiny861";
const char attiny87[] PROGMEM = "ATtiny87";
const char attiny167[] PROGMEM = "ATtiny167";
const char attiny828[] PROGMEM = "ATtiny828";
const char attiny48[] PROGMEM = "ATtiny48";
const char attiny88[] PROGMEM = "ATtiny88";
const char attiny1634[] PROGMEM = "ATtiny1634";
const char atmega48a[] PROGMEM = "ATmega48A";
const char atmega48pa[] PROGMEM = "ATmega48PA";
const char atmega48pb[] PROGMEM = "ATmega48PB";
const char atmega88a[] PROGMEM = "ATmega88A";
const char atmega88pa[] PROGMEM = "ATmega88PA";
const char atmega88pb[] PROGMEM = "ATmega88PB";
const char atmega168a[] PROGMEM = "ATmega168A";
const char atmega168pa[] PROGMEM = "ATmega168PA";
const char atmega168pb[] PROGMEM = "ATmega168PB";
const char atmega328[] PROGMEM = "ATmega328";
const char atmega328p[] PROGMEM = "ATmega328P";
const char atmega328pb[] PROGMEM = "ATmega328PB";
const char atmega8u2[] PROGMEM = "ATmega8U2";
const char atmega16u2[] PROGMEM = "ATmega16U2";
const char atmega32u2[] PROGMEM = "ATmega32U2";
const char atmega32c1[] PROGMEM = "ATmega32C1";
const char atmega64c1[] PROGMEM = "ATmega64C1";
const char atmega16m1[] PROGMEM = "ATmega16M1";
const char atmega32m1[] PROGMEM = "ATmega32M1";
const char atmega64m1[] PROGMEM = "ATmega64M1";
const char at90usb82[] PROGMEM = "AT90USB82";
const char at90usb162[] PROGMEM = "AT90USB162";
const char at90pwm12b3b[] PROGMEM = "AT90PWM1/2B/3B";
const char at90pwm81[] PROGMEM = "AT90PWM81";
const char at90pwm161[] PROGMEM = "AT90PWM161";
const char at90pwm216316[] PROGMEM = "AT90PWM216/316";

const char Connected[] PROGMEM = "Connected to ";

//  параметры микроконтроллеров
struct {
  unsigned int sig;        // двухбайтовая подпись
  bool      avreplus;      // представляет собой микроконтроллер с архитектурой AVRe+ (включает инструкцию MUL)
  unsigned int ramsz;      // Размер SRAM в байтах
  unsigned int rambase;    // базовый адрес SRAM
  unsigned int eepromsz;   // размер EEPROM в байтах
  unsigned int flashsz;    // размер флэш-памяти в байтах
  byte         dwdr;       // адрес регистра DWDR
  unsigned int pagesz;     // размер страницы флэш-памяти в байтах
  bool      erase4pg;      // 1 когда микроконтроллер имеет операцию стирания 4 страниц.
  unsigned int bootaddr;   // наибольший адрес возможного загрузочного раздела (0, если нет поддержки загрузки)
  byte         eecr;       // адрес реестра EECR
  byte         eearh;      // адрес регистра EARL (0, если нет)
  byte         rcosc;      // fuse для установки RC OSC в качестве источника синхронизации
  byte         extosc;     // fuse для установки EXTernal OSC в качестве источника синхронизации
  byte         xtalosc;    // fuse для установки XTAL OSC в качестве источника синхронизации
  byte         slowosc;    // fuse для настройки генератора 128 кГц
  const char*  name;       // указатель на имя в PROGMEM
  byte         dwenfuse;   // Маска для fuse DWEN в старшем байте регистра fuse
  byte         ckdiv8;     // битовая маска для fuse CKDIV8 в младшем байте регистра fuse
  byte         ckmsk;      // битовая маска для выбора источника синхронизации (clock source and startup time)
  byte         eedr;       // адрес EEDR (вычисляется из EECR)
  byte         eearl;      // адрес EARL (вычисляется из EECR)
  unsigned int targetpgsz; // целевой размер страницы (зависит от размера страницы и Erase4pg)
  byte         stuckat1byte;   // фиксированные биты в старшем байте программного счетчика
} mcu;


struct mcu_info_type {
  unsigned int sig;            // двухбайтовая подпись
  byte         ramsz_div_64;   // Размер SRAM
  bool         rambase_low;    // базовый адрес SRAM; низкий: 0x60, высокий: 0x100
  byte         eepromsz_div_64;// размер EEPROM
  byte         flashsz_div_1k; // размер flash memory
  byte         dwdr;           // адрес регистра DWDR
  byte         pagesz_div_2;   // размер страницы
  bool         erase4pg;       // 1, когда микроконтроллер имеет операцию стирания 4 страниц.
  unsigned int bootaddr;       // наибольший адрес возможного загрузочного раздела (0, если нет поддержки загрузки)
  byte         eecr;           // адрес реестра EECR
  byte         eearh;          // адрес регистра EARL (0, если нет)
  byte         rcosc;          // fuse для установки RC osc в качестве источника синхронизации
  byte         extosc;         // fuse для установки EXTernal OSC в качестве источника синхронизации
  byte         xtalosc;        // fuse для установки XTAL OSC в качестве источника синхронизации
  byte         slowosc;        // fuse для настройки генератора 128 кГц
  bool         avreplus;       // AVRe+ архитектура
  const char*  name;           // указатель на имя в PROGMEM
};

// информация о микроконтроллере (для всех микроконтроллеров AVR, поддерживающих debugWIRE)
// непроверенные отмечены звездочкой *
const mcu_info_type mcu_info[] PROGMEM = {
  // sig sram low eep flsh dwdr  pg er4 boot    eecr eearh rcosc extosc xtosc slosc plus name
  {0x9007,  1, 1,  1,  1, 0x2E,  16, 0, 0x0000, 0x1C, 0x00, 0x0A, 0x08, 0x00, 0x0B, 0, attiny13},

  {0x910A,  2, 1,  2,  2, 0x1f,  16, 0, 0x0000, 0x1C, 0x00, 0x24, 0x20, 0x3F, 0x26, 0, attiny2313},
  {0x920D,  4, 1,  4,  4, 0x27,  32, 0, 0x0000, 0x1C, 0x00, 0x24, 0x20, 0x3F, 0x26, 0, attiny4313},

  {0x920C,  4, 1,  1,  4, 0x27,  32, 0, 0x0000, 0x1C, 0x00, 0x22, 0x20, 0x3F, 0x23, 0, attiny43},

  {0x910B,  2, 1,  2,  2, 0x27,  16, 0, 0x0000, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x24, 0, attiny24},   
  {0x9207,  4, 1,  4,  4, 0x27,  32, 0, 0x0000, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x24, 0, attiny44},
  {0x930C,  8, 1,  8,  8, 0x27,  32, 0, 0x0000, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x24, 0, attiny84},
  
  {0x9215,  4, 0,  4,  4, 0x27,   8, 1, 0x0000, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x00, 0, attiny441}, 
  {0x9315,  8, 0,  8,  8, 0x27,   8, 1, 0x0000, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x00, 0, attiny841},
  
  {0x9108,  2, 1,  2,  2, 0x22,  16, 0, 0x0000, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x24, 0, attiny25},
  {0x9206,  4, 1,  4,  4, 0x22,  32, 0, 0x0000, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x24, 0, attiny45},
  {0x930B,  8, 1,  8,  8, 0x22,  32, 0, 0x0000, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x24, 0, attiny85},
  
  {0x910C,  2, 1,  2,  2, 0x20,  16, 0, 0x0000, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x23, 0, attiny261},
  {0x9208,  4, 1,  4,  4, 0x20,  32, 0, 0x0000, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x23, 0, attiny461},
  {0x930D,  8, 1,  8,  8, 0x20,  32, 0, 0x0000, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x23, 0, attiny861},
  
  {0x9387,  8, 0,  8,  8, 0x31,  64, 0, 0x0000, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 0, attiny87},  
  {0x9487,  8, 0,  8, 16, 0x31,  64, 0, 0x0000, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 0, attiny167},

  {0x9314,  8, 0,  4,  8, 0x31,  32, 0, 0x0F80, 0x1F, 0x22, 0x3E, 0x2C, 0x3E, 0x00, 0, attiny828},

  {0x9209,  4, 0,  1,  4, 0x31,  32, 0, 0x0000, 0x1F, 0x22, 0x2E, 0x2C, 0x00, 0x23, 0, attiny48},  
  {0x9311,  8, 0,  1,  8, 0x31,  32, 0, 0x0000, 0x1F, 0x22, 0x2E, 0x2C, 0x00, 0x23, 0, attiny88},
  
  {0x9412, 16, 0,  4, 16, 0x2E,  16, 1, 0x0000, 0x1C, 0x00, 0x02, 0x00, 0x0F, 0x00, 0, attiny1634},
  
  {0x9205,  8, 0,  4,  4, 0x31,  32, 0, 0x0000, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega48a},
  {0x920A,  8, 0,  4,  4, 0x31,  32, 0, 0x0000, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega48pa},
  {0x9210,  8, 0,  4,  4, 0x31,  32, 0, 0x0000, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega48pb}, // *
  {0x930A, 16, 0,  8,  8, 0x31,  32, 0, 0x0F80, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega88a},
  {0x930F, 16, 0,  8,  8, 0x31,  32, 0, 0x0F80, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega88pa},
  {0x9316, 16, 0,  8,  8, 0x31,  32, 0, 0x0F80, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega88pb}, // *
  {0x9406, 16, 0,  8, 16, 0x31,  64, 0, 0x1F80, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega168a},
  {0x940B, 16, 0,  8, 16, 0x31,  64, 0, 0x1F80, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega168pa},
  {0x9415, 16, 0,  8, 16, 0x31,  64, 0, 0x1F80, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega168pb}, // *
  {0x9514, 32, 0, 16, 32, 0x31,  64, 0, 0x3F00, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega328},
  {0x950F, 32, 0, 16, 32, 0x31,  64, 0, 0x3F00, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega328p},
  {0x9516, 32, 0, 16, 32, 0x31,  64, 0, 0x3F00, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x23, 1, atmega328pb},
  
  {0x9389,  8, 0,  8,  8, 0x31,  32, 0, 0x0000, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00, 1, atmega8u2},   // *
  {0x9489,  8, 0,  8, 16, 0x31,  64, 0, 0x0000, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00, 1, atmega16u2},  // *
  {0x958A, 16, 0, 16, 32, 0x31,  64, 0, 0x0000, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00, 1, atmega32u2},  // *

  {0x9484, 16, 0,  8, 16, 0x31,  64, 0, 0x1F00, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00, 1, atmega16m1},  // *
  {0x9586, 32, 0, 16, 32, 0x31,  64, 0, 0x3F00, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00, 1, atmega32c1},  // *
  {0x9584, 32, 0, 16, 32, 0x31,  64, 0, 0x3F00, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00, 1, atmega32m1},  // *
  {0x9686, 64, 0, 32, 64, 0x31, 128, 0, 0x3F00, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00, 1, atmega64c1},  // *
  {0x9684, 64, 0, 32, 64, 0x31, 128, 0, 0x3F00, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00, 1, atmega64m1},  // *

  {0x9382,  8, 0,  8,  8, 0x31,  64, 0, 0x1E00, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00, 1, at90usb82},   // *
  {0x9482,  8, 0,  8, 16, 0x31,  64, 0, 0x3E00, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00, 1, at90usb162},  // *

  {0x9383,  8, 0,  8,  8, 0x31,  32, 0, 0x0F80, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00,  1, at90pwm12b3b},// * 

  {0x9388,  4, 0,  8,  8, 0x31,  32, 0, 0x0F80, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x23, 1, at90pwm81},  // *
  {0x948B, 16, 0,  8, 16, 0x31,  64, 0, 0x1F00, 0x1C, 0x1F, 0x22, 0x20, 0x3F, 0x23, 1, at90pwm161}, // *

  {0x9483, 16, 0,  8, 16, 0x31,  64, 0, 0x1F00, 0x1F, 0x22, 0x22, 0x20, 0x3F, 0x00, 1, at90pwm216316},  // *
  {0,      0,  0,  0, 0,  0,     0,  0, 0,      0,    0,    0,    0,    0,    0,   0, 0},
};

const byte maxspeedexp = 4; // соответствует коэффициенту 16
const byte speedcmd[] PROGMEM = { 0x83, 0x82, 0x81, 0x80, 0xA0, 0xA1 };
unsigned long speedlimit = SPEEDHIGH;

enum Fuses { CkDiv8, CkDiv1, CkRc, CkXtal, CkExt, CkSlow, Erase, DWEN };

const int maxbpsix = 5;
// Обратите внимание, что Uno может общаться по номеру (номинально) 230400, а Nano — нет.
// Более глубокая причина в том, что ATmega328 на частоте 16 МГц фактически на 3,6% медленнее
// при попытке связаться по номеру 230400
// (см. https://hinterm-ziel.de/index.php/2021/10/26/communicating-asynchronous/).
// На Uno ATmega16U2 творит чудеса и снижает скорость связи до
// 220000, тогда как на Nano чип FTDI или CH340 отправляет вещи по номинальной стоимости,
// это слишком быстро для ATmega.

// немного статистики
long timeoutcnt = 0;  // счетчик таймаутов чтения DW
long flashcnt = 0;    // счетчик записей флэш-памяти
#if FREERAM
int freeram = 2048;   // минимальное количество свободной памяти (только если включено)
#define measureRam() freeRamMin()
#else
#define measureRam()
#endif

// интерфейс связи с целью
byte          lastsignal;

// связь и буфер памяти
byte membuf[MAXMEMBUF];     // используется для хранения значений SRAM, Flash и EEPROM
byte newpage[MAXPAGESIZE];  // одна страница флешки для программы
byte page[MAXPAGESIZE];     // содержимое кэшированной страницы — никогда не перезаписывайте его в программе! 
unsigned int lastpg;        // адрес кэшированной страницы
bool validpg = false;    // если содержимое кэшированной страницы действительно
bool flashidle;          // программирование флэш-памяти не активно
unsigned int flashpageaddr; // текущая страница для программирования следующей
byte buf[MAXBUF+1];         // для gdb i/o
int buffill;                // какая часть буфера заполнена
byte fatalerror = NO_FATAL;

#include "main.h"

SPIClass SPI_1(MOSI1, MISO1, SCLK1);
SPIClass SPI_2(MOSI2, MISO2, SCLK2);

//-------------------------------------------------------
// настройка платы
//-------------------------------------------------------  
void setup() {
  pinMode(SNSGND, INPUT_PULLUP);
  ctx.hostbps = INITIALBPS;
  pinMode(LEDGND, OUTPUT);
  pinMode(SYSLED, OUTPUT);
  digitalWrite(LEDGND, LOW);
  pinMode(VSUP, OUTPUT);
  digitalWrite(VSUP, HIGH);
  initSession();                    // инициализировать все критические глобальные переменные
  pinMode(TISP, OUTPUT);
  digitalWrite(TISP, HIGH);         // выключаем линии ISP 
  pinMode(DWLINE, INPUT);           // освободим линию RESET, чтобы разрешить запуск debugWIRE
  
  Serial.begin(ctx.hostbps);
  SPI.begin();
  SPI_2.begin(); 
  SPI.setClockDivider(SPI_CLOCK_DIV16); // Sets clock for SPI communication at 16 (72/16=4.5Mhz)
}

//-------------------------------------------------------
// главный цикл программы
//-------------------------------------------------------
void loop() {
    // Мониторим состояние загрузки системы
    monitorSystemLoadState();

    // Проверяем, доступны ли данные по последовательному порту
    if (Serial.available()) {
        // Если данные доступны, обрабатываем их как команды gdb
        gdbHandleCmd();
    } else {
        // Если данных по последовательному порту нет
        // и система находится в состоянии выполнения (RUN_STATE)
        if (ctx.state == RUN_STATE) {
            // Проверяем, доступны ли данные по второму последовательному порту (Serial2)
            if (Serial2.available()) {
                // Считываем байт из Serial2
                byte cc = Serial2.read();
                // Если считанный байт равен 0x0 (break установлен target-ом)
                if (cc == 0x0) {
                    // Если ожидается калибровка, выполняем следующие действия
                    if (expectUCalibrate()) {
                        // Остановка выполнения
                        delayMicroseconds(5); // Пауза для избежания конфликтов на линии
                        // Отправляем состояние SIGTRAP через gdb
                        gdbSendState(SIGTRAP);
                    }
                }
            }
        }
    }
}

/****************** процедуры состояния системы ***********************/
/*
void sendBreak(void)
{
  enable(false);
  //ICDDR |= _BV(ICBIT); // переключите контакт PB0 на выход (который всегда низкий)
  //delay(120); // достаточно для 100 бит/с
  //ICDDR &= ~_BV(ICBIT); // и переключите PB0 обратно на вход
  enable(true);
}
*/

unsigned long calibrate()
{
  return 1;
}

bool overflow()
{
/*  bool ret = _buffer_overflow;
  if (ret)
    _buffer_overflow = false;
  return ret; */
  return true;
}

size_t sendCmd(const uint8_t *loc, uint8_t len) {
  return Serial2.write(loc, len);
} 

/*
void enable(bool active)
{
  //setRxIntMsk(active); // разрешить или запретить прием
}
*/

// Мониторим статус системы LOAD_STATE
// Если больше нет входных данных, то очищаем буфер флэш-страницы и
// возвращаем состояние = подключено
void monitorSystemLoadState(void) {
    static unsigned int noinput = 0; // Статическая переменная для отслеживания времени без входных данных

    // Увеличиваем счетчик времени без входных данных, если данные по последовательному порту не доступны
    if (Serial.available())
        noinput = 0;
    noinput++;

    // Проверяем, прошло ли примерно 50 мс (2777 циклов, основываясь на том, что один цикл составляет 18 мкс)
    if (noinput == 2777) {
        // Если система находится в состоянии LOAD_STATE
        if (ctx.state == LOAD_STATE) {
            // Если скорость связи превышает 30000, сбрасываем буфер флэш-программы
            if (ctx.bps >= 30000)
                targetFlushFlashProg();

            // Устанавливаем состояние системы в CONN_STATE (подключено)
            setSysState(CONN_STATE);
        }
    }
}

// Инициализируем все глобальные переменные при подключении отладчика
void initSession(void)
{
  flashidle = true;                         // Устанавливаем флаг flashidle в значение true (свободен для записи во флэш-память)
  ctx.safestep = true;                      // Устанавливаем флаг safestep в значение true (безопасный режим выполнения шага)
  bpcnt = 0;                                // Обнуляем счетчики точек останова
  bpused = 0;
  hwbp = 0xFFFF;                            // Инициализируем аппаратные точки останова
  lastsignal = SIGTRAP;                     // Устанавливаем последний сигнал в SIGTRAP (последняя ошибка - SIGTRAP)
  validpg = false;                          // Устанавливаем флаг validpg в значение false (нет действительной страницы)
  buffill = 0;                              // Обнуляем буфер
  fatalerror = NO_FATAL;                    // Устанавливаем флаг фатальной ошибки в NO_FATAL (нет фатальных ошибок)
  setSysState(NOTCONN_STATE);               // Устанавливаем состояние системы в NOTCONN_STATE (не подключено)
  targetInitRegisters();                    // Инициализируем регистры целевой платформы
}

// Сообщаем о фатальной ошибке и останавливаем все операции
// Ошибка будет отображаться при попытке выполнения
// Если для параметра checkio установлено значение true, мы проверим,
// соединение с целью все еще существует
// Если нет, то ошибка не записывается, но соединение есть
// помечено как не подключенное
void reportFatalError(byte errnum, bool checkio)
{
  // Если требуется проверка соединения с целью и цель находится в автономном режиме, игнорируем ошибку
  if (checkio) {
    if (targetOffline()) return;
    // Если не удалось выполнить калибровку, цель больше не подключена
    if (!expectUCalibrate()) {
      setSysState(NOTCONN_STATE); // Устанавливаем состояние как не подключено и игнорируем ошибку
      return;
    }
  }

  // Если фатальная ошибка еще не была записана, устанавливаем переданное значение
  if (fatalerror == NO_FATAL)
    fatalerror = errnum;

  setSysState(ERROR_STATE);                 // Устанавливаем состояние системы в ERROR_STATE
}

// Меняем состояние системы
// Включаем мигание IRQ при запуске, ошибке или состоянии выключения и включения питания
void setSysState(statetype newstate)
{
  // Если система находится в состоянии ошибки и установлена фатальная ошибка, выходим
  if (ctx.state == ERROR_STATE && fatalerror) return;

  // Устанавливаем новое состояние системы
  ctx.state = newstate;

  // Устанавливаем времена включения и выключения для мигания индикатора
  ontime = ontimes[newstate];
  offtime = offtimes[newstate];

  // Устанавливаем режим вывода для системного LED
  pinMode(SYSLED, OUTPUT);

  // Если время включения равно 0, выключаем индикатор
  if (ontimes[newstate] == 0) digitalWrite(SYSLED, LOW);
  // Если время выключения равно 0, включаем индикатор
  else if (offtimes[newstate] == 0) digitalWrite(SYSLED, HIGH);
  // В противном случае настраиваем мигание индикатора
  else {
    // Задаем значение регистра сравнения для таймера
    // и включаем прерывание по совпадению (IRQ)
    // (закомментированный код отключен)
    // OCR0A = 0x80;
    // TIMSK0 |= _BV(OCIE0A);
  }
}

/****************** Подпрограммы GDB RSP **************************/

// Обрабатываем команду от клиента GDB RSP
void gdbHandleCmd(void)
{
  byte checksum, pkt_checksum;
  byte b;

  // Измеряем использование памяти (неопределенный метод)
  measureRam();

  // Считываем первый байт команды от клиента GDB
  b = gdbReadByte();

  switch(b) {
  case ':':
    // Начало нового пакета данных
    buffill = 0;

    // Считываем данные до символа '#' или до заполнения буфера
    for (pkt_checksum = 0, b = gdbReadByte(); b != '#' && buffill < MAXBUF; b = gdbReadByte()) {
      buf[buffill++] = b;
      pkt_checksum += b;
    }
    buf[buffill] = 0; // Завершаем строку нулевым символом

    // Считываем контрольную сумму из пакета
    checksum  = hex2nib(gdbReadByte()) << 4;
    checksum |= hex2nib(gdbReadByte());

    // Проверяем контрольную сумму пакета
    // Если не совпадает, отправляем NACK (отрицательный ответ)
    if (pkt_checksum != checksum) {
      gdbSendByte('-');
      return;
    }

    // Отправляем ACK (положительный ответ)
    gdbSendByte('+');

    // Анализируем полученный буфер (и, возможно, начинаем выполнение программы)
    gdbParsePacket(buf);
    break;

  case '-':  // Негативный ответ (NACK), повторяем предыдущий ответ
    gdbSendBuff(buf, buffill);
    break;

  case '+':  // Положительный ответ (ACK), ничего не делаем
    break;

  case 0x03:
    // Пользовательское прерывание по Ctrl-C
    // Отправляем текущее состояние и продолжаем выполнение программы
    if (ctx.state == RUN_STATE) {
      targetBreak();  // Останавливаем выполнение программы на целевом устройстве
      if (expectUCalibrate()) {
        gdbSendState(SIGINT);  // Отправляем сигнал прерывания (SIGINT) в GDB
      } else {
        gdbSendState(SIGHUP);  // Отправляем сигнал завершения (SIGHUP) в GDB
      }
    }
    break;

  default:
    // Просто игнорируем неизвестные команды, так как ожидаем только записи данных или ACK/NACK
    break;
  }
}

// Анализируем пакет и, возможно, начинаем выполнение
void gdbParsePacket(const byte *buff)
{
  byte s;

  // Проверяем, заняты ли операции с флеш-памятью, и завершаем их при необходимости
  if (!flashidle) {
    if (*buff != 'X' && *buff != 'M')
      targetFlushFlashProg();  // Завершаем программирование флеш-памяти, если текущая операция не связана с записью
  } else {
    if (*buff == 'X' || *buff == 'M')
      gdbUpdateBreakpoints(true);  // Удаляем все точки останова перед записью во флеш
  }

  // Обрабатываем различные команды в зависимости от первого символа в пакете данных
  switch (*buff) {
  case '?':  // Запрос о последнем сигнале
    gdbSendSignal(lastsignal);
    break;
  case 'H':  // Устанавливаем поток, всегда отвечаем "OK"
    gdbSendReply("OK");
    break;
  case 'T':  // Поток активен, всегда отвечаем "OK"
    gdbSendReply("OK");
    break;
  case 'g':  // Чтение регистров
    gdbReadRegisters();
    break;
  case 'G':  // Запись регистров
    gdbWriteRegisters(buff + 1);
    break;
  case 'm':  // Чтение памяти
    gdbReadMemory(buff + 1);
    break;
  case 'M':  // Запись в память
    gdbWriteMemory(buff + 1, false);
    break;
  case 'X':  // Запись двоичных данных в память
    gdbWriteMemory(buff + 1, true);
    break;
  case 'D':  // Отключение отладчика
    // Удаляем точки останова в памяти перед выходом
    gdbUpdateBreakpoints(true);
    validpg = false;
    fatalerror = NO_FATAL;
    targetContinue();  // Позволяем целевому устройству продолжить выполнение
    setSysState(NOTCONN_STATE);  // Устанавливаем состояние "неподключено"
    gdbSendReply("OK");  // Отправляем подтверждение
    break;
  // Обработка других команд...
  default:
    gdbSendReply("");  // Неизвестная команда, отвечаем пустым сообщением
    break;
  }
}

void gdbParseMonitorPacket(const byte *buf)
{
   [[maybe_unused]] int para = 0;

  measureRam();

  gdbUpdateBreakpoints(true);                         // обновляем точки останова в памяти перед любыми операциями монитора

  int clen = strlen((const char *)buf); 
  
  if (memcmp_P(buf, (void *)PSTR("64776f666600"), max(6,min(12,clen))) == 0)                  
    gdbStop();                                                              /* dwo[ff] */
  else if (memcmp_P(buf, (void *)PSTR("6477636f6e6e65637400"), max(6,min(20,clen))) == 0)
    if (gdbConnect(true)) gdbSendReply("OK");                               /* dwc[onnnect] */
    else gdbSendReply("E03");
  else if (memcmp_P(buf, (void *)PSTR("68656c7000"), max(4,min(10,clen))) == 0)                  
    gdbHelp();                                                              /* he[lp] */
  else if (memcmp_P(buf, (void *)PSTR("73657269616c00"), max(6,min(14,clen))) == 0)
    gdbReportRSPbps();                                                       /* serial */
  else if (memcmp_P(buf, (void *)PSTR("666c617368636f756e7400"), max(6,min(22,clen))) == 0)
    gdbReportFlashCount();                                                  /* fla[shcount] */
  else if (memcmp_P(buf, (void *)PSTR("72616d757361676500"), max(6,min(18,clen))) == 0)
    gdbReportRamUsage();                                                    /* ram[usage] */
  else if (memcmp_P(buf, (void *)PSTR("636b387072657363616c657200"), max(6,min(26,clen))) == 0)
    gdbSetFuses(CkDiv8);                                                    /* ck8[prescaler] */
  else if (memcmp_P(buf, (void *)PSTR("636b317072657363616c657200"), max(6,min(26,clen))) == 0) 
    gdbSetFuses(CkDiv1);                                                    /* ck1[prescaler] */
  else if (memcmp_P(buf, (void *)PSTR("72636f736300"), max(4,min(12,clen))) == 0)
    gdbSetFuses(CkRc);                                                      /* rc[osc] */
  else if (memcmp_P(buf, (void *)PSTR("6578746f736300"), max(4,min(14,clen))) == 0)
    gdbSetFuses(CkExt);                                                     /* ex[tosc] */
  else if (memcmp_P(buf, (void *)PSTR("7874616c6f736300"), max(4,min(16,clen))) == 0)
    gdbSetFuses(CkXtal);                                                     /* xt[alosc] */
  else if (memcmp_P(buf, (void *)PSTR("736c6f776f736300"), max(4,min(16,clen))) == 0)
    gdbSetFuses(CkSlow);                                                     /* sl[owosc] */
  else if (memcmp_P(buf, (void *)PSTR("657261736500"), max(4,min(12,clen))) == 0)
    gdbSetFuses(Erase);                                                     /*er[ase]*/
  else if (memcmp_P(buf, (void *)PSTR("6877627000"), max(4,min(10,clen))) == 0)
    gdbSetMaxBPs(1);                                                        /* hw[bp] */
  else if (memcmp_P(buf, (void *)PSTR("7377627000"), max(4,min(10,clen))) == 0)
    gdbSetMaxBPs(MAXBREAK);                                                 /* sw[bp] */
  else if (memcmp_P(buf, (void *)PSTR("34627000"), max(2,min(8,clen))) == 0)
    gdbSetMaxBPs(4);                                                        /* 4[bp] */
  else if (memcmp_P(buf, (void *)PSTR("7370"), 4) == 0)
    gdbSetSpeed(buf);                                                       /* sp[eed] [h|l] */
  else if (memcmp_P(buf, (void *)PSTR("736166657374657000"), max(4,min(18,clen))) == 0)
    gdbSetSteppingMode(true);                                               /* safestep */
  else if (memcmp_P(buf, (void *)PSTR("756e736166657374657000"), max(4,min(12,clen))) == 0)
    gdbSetSteppingMode(false);                                              /* unsafestep */
  else if (memcmp_P(buf, (void *)PSTR("76657273696f6e00"), max(4,min(16,clen))) == 0) 
    gdbVersion();                                                           /* version */
  else if (memcmp_P(buf, (void *)PSTR("74696d656f75747300"), max(8,min(18,clen))) == 0)
    gdbTimeoutCounter();                                                    /* timeouts */
  else if (memcmp_P(buf, (void *)PSTR("726573657400"), max(4,min(12,clen))) == 0) {
    if (gdbReset()) gdbSendReply("OK");                                     /* re[set] */
    else gdbSendReply("E09");
  } else gdbSendReply("");
}

// справочная функция
inline void gdbHelp(void) {
  gdbDebugMessagePSTR(PSTR("monitor help         - help function"), -1);
  gdbDebugMessagePSTR(PSTR("monitor dwconnect    - connect to target and show parameters (*)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor dwoff        - disconnect from target and disable DWEN (*)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor eraseflash   - erase flash memory (*)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor reset        - reset target (*)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor ck1prescaler - unprogram CK8DIV (*)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor ck8prescaler - program CK8DIV (*)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor extosc       - use external clock source (*)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor xtalosc      - use XTAL as clock source (*)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor rcosc        - use internal RC oscillator as clock source (*)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor slosc        - use internal 128kHz oscillator as clock source (*)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor swbp         - allow 32 software breakpoints (default)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor hwbp         - allow only 1 breakpoint, i.e., the hardware bp"), -1);
  gdbDebugMessagePSTR(PSTR("monitor safestep     - prohibit interrupts while single-stepping(default)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor unsafestep   - allow interrupts while single-stepping"), -1);
  gdbDebugMessagePSTR(PSTR("monitor speed [h|l]  - speed limit is h (=250kbps) (def.) or l (=125kbps)"), -1);
  gdbDebugMessagePSTR(PSTR("monitor serial       - report host communication speed"), -1);
  gdbDebugMessagePSTR(PSTR("monitor flashcount   - report number of flash pages written since start"), -1);
  gdbDebugMessagePSTR(PSTR("monitor timeouts     - report timeouts"), -1);
  gdbDebugMessagePSTR(PSTR("monitor version      - report version number"), -1);
  gdbDebugMessagePSTR(PSTR("All commands with (*) lead to a reset of the target"), -1);
  gdbSendReply("OK");
}

// возвращаем счетчик таймаута
inline void gdbTimeoutCounter(void)
{
  gdbReplyMessagePSTR(PSTR("Number of timeouts: "), timeoutcnt);
}


// устанавливаем режим шагания
inline void gdbSetSteppingMode(bool safe)
{
  ctx.safestep = safe;
  if (safe)
    gdbReplyMessagePSTR(PSTR("Single-stepping now interrupt-free"), -1);
  else
    gdbReplyMessagePSTR(PSTR("Single-stepping now interruptible"), -1);
}

// показываем версию
inline void gdbVersion(void)
{
  gdbReplyMessagePSTR(PSTR("dw-link V" VERSION), -1);
}
  
//показываем скорость соединения с хостом
inline void gdbReportRSPbps(void)
{
  gdbReplyMessagePSTR(PSTR("Current bitrate of serial connection to host: "), ctx.hostbps);
}

// получаем скорость DW
inline void gdbGetSpeed(void)
{
  gdbReplyMessagePSTR(PSTR("Current debugWIRE bitrate: "), ctx.bps);
}

// устанавливаем скорость связи DW
void gdbSetSpeed(const byte cmd[])
{
  byte arg;
  byte argix = findArg(cmd);
  if (argix == 0) {
    gdbSendReply("");
    return;
  }
  if (cmd[argix] == '\0') arg = '\0';
  else arg = (hex2nib(cmd[argix])<<4) + hex2nib(cmd[argix+1]);
  switch (arg) {
  case 'h': speedlimit = SPEEDHIGH;
    break;
  case 'l': speedlimit = SPEEDLOW;
    break;
  case '\0': gdbGetSpeed();
    return;
  default:
    gdbSendReply("");
  }
  doBreak();
  gdbGetSpeed();
  return;
}

byte findArg(const byte cmd[])
{
  byte ix = 4;
  if (cmd[ix] =='2' && cmd[ix+1] == '0') return ix + 2;
  if (cmd[ix] =='\0') return ix;
  if (cmd[ix] != '6' || cmd[ix+1] != '5')  return 0;
  ix += 2;
  if (cmd[ix] =='2' && cmd[ix+1] == '0') return ix + 2;
  if (cmd[ix] == '\0') return ix;
  if (cmd[ix] != '6' || cmd[ix+1] != '5')  return 0;
  ix +=2;
  if (cmd[ix] =='2' && cmd[ix+1] == '0') return ix + 2;
  if (cmd[ix] =='\0') return ix;
  if (cmd[ix] != '6' || cmd[ix+1] != '4')  return 0;
  ix += 2;
  if (cmd[ix] =='2' && cmd[ix+1] == '0') return ix + 2;
  if (cmd[ix] =='\0') return ix;
  return 0;
}

// "мониторить swbp/hwbp/4bp"
// устанавливаем максимальное количество точек останова
inline void gdbSetMaxBPs(byte num)
{
  maxbreak = num;
  gdbReplyMessagePSTR(PSTR("Maximum number of breakpoints now: "), num);
}

// "отслеживать количество флэш-счетов"
// отчет о том, сколько флеш-страниц было записано
inline void gdbReportFlashCount(void)
{
  gdbReplyMessagePSTR(PSTR("Number of flash write operations: "), flashcnt);
}

// "мониторить ramusage"
inline void gdbReportRamUsage(void)
{
#if FREERAM
  gdbReplyMessagePSTR(PSTR("Minimal number of free RAM bytes: "), freeram);
#else
  gdbSendReply("");
#endif
}


// "мониторить dwconnect"
// пытаемся включить debugWIRE
// это может означать, что пользователю необходимо выключить и включить целевую систему
bool gdbConnect(bool verbose)
{
  int conncode = -CONNERR_UNKNOWN;

  delay(100);                         // изначально разрешаем запуск МК
  mcu.sig = 0;
  if (targetDWConnect()) {
    conncode = 1;
  } else {
    conncode = targetISPConnect();
    if (conncode == 0) {
      if (powerCycle(verbose)) 
	      conncode = 1;
      else
	      conncode = -1;
    }
  }
  if (conncode == 1) {
    mcu.stuckat1byte = 0;
    if (DWgetWPc(false) > (mcu.flashsz>>1)) conncode = -4;
  }
  if (conncode == 1) {
    setSysState(CONN_STATE);
    if (verbose) {
      gdbDebugMessagePSTR(Connected,-2);
      gdbDebugMessagePSTR(PSTR("debugWIRE is enabled, bps: "),ctx.bps);
    }
    gdbCleanupBreakpointTable();
    return true;
  }
  if (verbose) {
    switch (conncode) {
    case -1: gdbDebugMessagePSTR(PSTR("Cannot connect: Check wiring"),-1); break;
    case -2: gdbDebugMessagePSTR(PSTR("Cannot connect: Unsupported MCU"),-1); break;
    case -3: gdbDebugMessagePSTR(PSTR("Cannot connect: Lock bits are set"),-1); break;
    case -4: gdbDebugMessagePSTR(PSTR("Cannot connect: PC with stuck-at-one bits"),-1); break;
    default: gdbDebugMessagePSTR(PSTR("Cannot connect for unknown reasons"),-1); conncode = -CONNERR_UNKNOWN; break;
    }
  }
  if (fatalerror == NO_FATAL) fatalerror = -conncode;
  setSysState(ERROR_STATE);
  flushInput();
  return false;
}

// выключаем и периодически проверяем, возможно ли это
// чтобы установить соединение debugWIRE, возвращаем false по истечении времени ожидания
bool powerCycle(bool verbose)
{
//  enable(false);
  setSysState(PWRCYC_STATE);
  delay(1000);
//  enable(true);
  if (targetDWConnect()) {
    setSysState(CONN_STATE);
    return true;
  }
  return false;
}

// "monitor dwoff" 
// попытаемся отключить интерфейс debugWIRE в целевой системе
void gdbStop(void)
{
  if (targetStop()) {
    gdbDebugMessagePSTR(Connected,-2);
    gdbReplyMessagePSTR(PSTR("debugWIRE is disabled"),-1);
    setSysState(NOTCONN_STATE);
  } else {
    gdbDebugMessagePSTR(PSTR("debugWIRE could NOT be disabled"),-1);
    gdbSendReply("E05");
  }
}

// "monitor reset"
// попытаемся сбросить target
bool gdbReset(void)
{
  if (targetOffline()) {
    gdbDebugMessagePSTR(PSTR("Target offline: Cannot reset"), -1);
    return false;
  }
  targetReset();
  targetInitRegisters();
  return true;
}

// "monitor ck1prescaler/ck8prescaler/rcosc/extosc"
void gdbSetFuses(Fuses fuse)
{
  bool offline = targetOffline();
  int res; 

  setSysState(NOTCONN_STATE);
  res = targetSetFuses(fuse);
  if (res < 0) {
    if (res == -1) gdbDebugMessagePSTR(PSTR("Cannot connect: Check wiring"),-1);
    else if (res == -2) gdbDebugMessagePSTR(PSTR("Unsupported MCU"),-1);
    else if (res == -3) gdbDebugMessagePSTR(PSTR("Fuse programming failed"),-1);
    else if (res == -4) gdbDebugMessagePSTR(PSTR("XTAL not possible"),-1);
    else if (res == -5) gdbDebugMessagePSTR(PSTR("128 kHz osc. not possible"),-1);
    flushInput();
    gdbSendReply("E05");
    return;
  }
  switch (fuse) {
  case CkDiv8: gdbDebugMessagePSTR(PSTR("CKDIV8 fuse is now programmed"),-1); break;
  case CkDiv1: gdbDebugMessagePSTR(PSTR("CKDIV8 fuse is now unprogrammed"),-1); break;
  case CkRc: gdbDebugMessagePSTR(PSTR("Using RC oscillator"),-1); break;
  case CkExt: gdbDebugMessagePSTR(PSTR("Using EXTernal oscillator"),-1); break;
  case CkXtal: gdbDebugMessagePSTR(PSTR("Using XTAL oscillator"),-1); break;
  case CkSlow: gdbDebugMessagePSTR(PSTR("Using 128 kHz oscillator"),-1); break;
  case Erase: gdbDebugMessagePSTR(PSTR("Chip erased"),-1); break;
  default: reportFatalError(WRONG_FUSE_SPEC_FATAL, false); gdbDebugMessagePSTR(PSTR("Fatal Error: Wrong fuse!"),-1); break;
  }
  if (!offline) gdbDebugMessagePSTR(PSTR("Reconnecting ..."),-1);
  delay(200);
  flushInput();
  if (offline) {
    gdbSendReply("OK");
    return;
  }
  if (!gdbConnect(true))
    gdbSendReply("E02");
  else 
    gdbSendReply("OK");
}

// извлекаем код операции и адрес текущего wpc (независимо от того, установлен ли там breakpoint)
void getInstruction(unsigned int &opcode, unsigned int &addr)
{
  opcode = 0;
  addr = 0;
  int bpix = gdbFindBreakpoint(ctx.wpc);
  if ((bpix < 0) || (!bp[bpix].inflash)) 
    opcode = targetReadFlashWord(ctx.wpc<<1);
  else
    opcode = bp[bpix].opcode;
  if (twoWordInstr(opcode)) {
    addr = targetReadFlashWord((ctx.wpc+1)<<1);
  }
}

#if OFFEX2WORD == 0
// проверяем, является ли опкод 32-битной инструкцией
bool twoWordInstr(unsigned int opcode)
{
  return(((opcode & ~0x1F0) == 0x9000) || ((opcode & ~0x1F0) == 0x9200) ||
	 ((opcode & 0x0FE0E) == 0x940C) || ((opcode & 0x0FE0E) == 0x940E));
}

inline void simTwoWordInstr(unsigned int opcode, unsigned int addr)
{
  if (twoWordInstr(opcode)) {
    byte reg, val;
    if ((opcode & ~0x1F0) == 0x9000) {   // lds 
      reg = (opcode & 0x1F0) >> 4;
      val = DWreadSramByte(addr);
      ctx.regs[reg] = val;
      ctx.wpc += 2;
    } else if ((opcode & ~0x1F0) == 0x9200) { // sts 
      reg = (opcode & 0x1F0) >> 4;
      DWwriteSramByte(addr,ctx.regs[reg]);
      ctx.wpc += 2;
    } else if ((opcode & 0x0FE0E) == 0x940C) { // jmp 
// поскольку debugWIRE работает только на микроконтроллерах с флэш-адресным пространством <= 64к слов
// нам не нужно использовать биты из опкода
      ctx.wpc = addr;
    } else if  ((opcode & 0x0FE0E) == 0x940E) { // call
      DWwriteSramByte(ctx.sp, (byte)((ctx.wpc+2) & 0xff)); // сохранить обратный адрес в стеке
      DWwriteSramByte(ctx.sp-1, (byte)((ctx.wpc+2)>>8));
      ctx.sp -= 2;                    // уменьшить указатель стека
      ctx.wpc = addr;                 // новоезначение указателя стека
    }
  }
}
#endif

// делаем один шаг
// начинаем с сохраненными регистрами и возвращаемся с сохраненными регистрами
// он вернет сигнал, который в случае успеха будет SIGTRAP
byte gdbStep(void)
{
  unsigned int opcode, arg;
  unsigned int oldpc = ctx.wpc;
  int bpix = gdbFindBreakpoint(ctx.wpc);
  byte sig = SIGTRAP;                 // SIGTRAP (normal), SIGILL (if ill opcode), SIGABRT (fatal)

  if (fatalerror) return SIGABRT;
  if (targetOffline()) return SIGHUP;
  getInstruction(opcode, arg);
  if (targetIllegalOpcode(opcode)) {
    return SIGILL;
  }
  if ((bpix >= 0 &&  bp[bpix].inflash) || ctx.safestep) {
    if (twoWordInstr(opcode)) 
      simTwoWordInstr(opcode, arg);
    else 
      {
	targetRestoreRegisters();
	DWexecOffline(opcode);
	targetSaveRegisters();
      }
  } else {
    targetRestoreRegisters();
    targetStep();
    if (!expectBreakAndU()) {
      ctx.saved = true;               // просто восстановить старое состояние
      reportFatalError(NO_STEP_FATAL, true);
      return SIGABRT;
    } else {
      targetSaveRegisters();
      if (oldpc == ctx.wpc) {
	if (Serial.available())
	  sig = SIGINT;                     // если мы не добьемся прогресса в поэтапном, ^C (или другие входы) может остановить gdb
      }
    }
  }
  return sig;
}

// начать выполнение с текущего программного счетчика
// если существует какая-то ошибка, верните соответствующий сигнал
// в противном случае отправьте команду в цель, чтобы начать выполнение и вернуть 0
byte gdbContinue(void)
{
  byte sig = 0;
  if (fatalerror) sig = SIGABRT;
  else if (targetOffline()) sig = SIGHUP;
  else {
    gdbUpdateBreakpoints(false);      // обновить точки останова во флэш-памяти
    if (targetIllegalOpcode(targetReadFlashWord(ctx.wpc*2))) {
      sig = SIGILL;
    }
  }
  if (sig) {                          // что-то пошло не так
    delay(2);
    return sig;
  }
  targetRestoreRegisters();
  setSysState(RUN_STATE);
  targetContinue();
  return 0;
}


// Удаляем неактивные и устанавливаем активные точки останова перед началом выполнения или перед сбросом/уничтожением/отсоединением.
// Обратите внимание, что GDB устанавливает точки останова непосредственно перед выполнением команды шага или продолжения и
// GDB удаляет точки останова сразу после остановки цели. Чтобы минимизировать износ вспышки,
// мы деактивируем точки останова, когда GDB захочет их удалить, но мы их не удаляем
// из флэш-памяти немедленно. Только перед тем, как цель начнет выполняться, мы обновим флэш-память.
// в зависимости от статуса точки останова:
// BP не используется (addr = 0) -> ничего не делать
// БП используется, активен и уже во флеше или hwbp -> ничего не делать
// БП используется, активен, не hwbp и не во флеше -> запись во флеш
// БП используется, но не активен и находится во флэше -> удалить из флэша, установить БП неиспользуемым
// BP используется, а не во флэш-памяти -> установить BP неиспользуемым
// BP используется, активен и hwbp -> ничего не делать, об этом позаботятся при выполнении
// Упорядочиваем все действующие БП по увеличению адреса и только потом меняем флэш-память
// (чтобы для нескольких BP на одной странице требовалось только одно изменение флэш-памяти)
//
// Если очистка параметра верна, мы также удалим инструкции BREAK
// активных точек останова, поскольку либо выход, либо действие записи в память
// следовать.
//
void gdbUpdateBreakpoints(bool cleanup)
{
  int i, j, ix, rel = 0;
  unsigned int relevant[MAXBREAK*2+1]; // word addresses соответствующих мест во flash
  unsigned int addr = 0;

  measureRam();

  if (!flashidle) {
    reportFatalError(BREAKPOINT_UPDATE_WHILE_FLASH_PROGRAMMING_FATAL, false);
    return;
  }
  
  // Update Breakpoints (used/active): bpused " / " bpcnt
  // если нет использованных записей, мы также можем немедленно вернуться
  // если цель не подключена, мы не можем обновить точки останова в любом случае
  if (bpused == 0 || targetOffline()) return;

  // найти подходящие breakpoints
  for (i=0; i < MAXBREAK*2; i++) {
    if (bp[i].used) {                 // только используемые breakpoints!
      if (bp[i].active) {             // активные breakpoint
	if (!cleanup) {
	  if (!bp[i].inflash && !bp[i].hw)  // еще не во флеше и не в hw bp
	    relevant[rel++] = bp[i].waddr;  // не забудьте установить
	} else { // active BP && cleanup
	  if (bp[i].inflash)                // удаляем инструкцию BREAK, но оставляем ее активной
	    relevant[rel++] = bp[i].waddr;
	}
      } else {                        // неактивный breakpoint
	if (bp[i].inflash) {                // все еще во flash 
	  relevant[rel++] = bp[i].waddr;    // не забудьте удалить
	} else {
	  bp[i].used = false;               // иначе освобожденный breakpoint уже сейчас
	  if (bp[i].hw) {                   // если hwbp, тогда освободим HWBP
	    bp[i].hw = false;
	    hwbp = 0xFFFF;
	  }
	  bpused--;
	}
      }
    }
  }
  relevant[rel++] = 0xFFFF;           // конец маркера
  // Relevant bps:  rel-1
  // отсортировать соответствующие breakpoint
  insertionSort(relevant, rel);
  // BPs sorted: for (i = 0; i < rel-1; i++) relevant[i]*2

  // заменяем страницы, которые необходимо изменить
  // обратите внимание, что все адреса в релевантных и bp являются адресами слов!
  i = 0;
  while (addr < mcu.flashsz && i < rel-1) {
    if (relevant[i]*2 >= addr && relevant[i]*2 < addr+mcu.targetpgsz) {
      j = i;
      while (relevant[i]*2 < addr+mcu.targetpgsz) i++;
      targetReadFlashPage(addr);
      memcpy(newpage, page, mcu.targetpgsz);
      while (j < i) {
	// RELEVANT: relevant[j]*2
	ix = gdbFindBreakpoint(relevant[j++]);
	if (ix < 0) reportFatalError(RELEVANT_BP_NOT_PRESENT, false);
	// Found BP: ix
	if (bp[ix].active && !cleanup) { // включено, но еще не во флэш-памяти && не выполняется операция очистки
	  bp[ix].opcode = (newpage[(bp[ix].waddr*2)%mcu.targetpgsz])+
	    (unsigned int)((newpage[((bp[ix].waddr*2)+1)%mcu.targetpgsz])<<8);
	  // Replace op: bp[ix].opcode; with BREAK at byte addr: bp[ix].waddr*2
	  bp[ix].inflash = true; 
				  
	  newpage[(bp[ix].waddr*2)%mcu.targetpgsz] = 0x98; // инструкция BREAK 
	  newpage[((bp[ix].waddr*2)+1)%mcu.targetpgsz] = 0x95;
	} else {                            // отключен, но все еще находится в режиме прошивки или очистки
	  // Restore original op: bp[ix].opcode; at byte addr: bp[ix].waddr*2
	  newpage[(bp[ix].waddr*2)%mcu.targetpgsz] = bp[ix].opcode&0xFF;
	  newpage[((bp[ix].waddr*2)+1)%mcu.targetpgsz] = bp[ix].opcode>>8;
	  bp[ix].inflash = false;
	  if (!bp[ix].active) {             // теперь освободим этот слот!
	    bp[ix].used = false;
	    bpused--;
	  }
	}
      }
      targetWriteFlashPage(addr);
    }
    addr += mcu.targetpgsz;
  }
  // After updating Breakpoints (used/active): bpused " / " bpcnt
  // HWBP = hwbp*2
}

// сортируем точки останова, чтобы их можно было изменять вместе, когда они находятся на одной странице
void insertionSort(unsigned int *seq, int len)
{
  unsigned int tmp;
  measureRam();
  for (int i = 1; i < len; i++) {
    for (int j = i; j > 0 && seq[j-1] > seq[j]; j--) {
      tmp = seq[j-1];
      seq[j-1] = seq[j];
      seq[j] = tmp;
    }
  }
}

// находим точку останова по заданному адресу 
int gdbFindBreakpoint(unsigned int waddr)
{
  measureRam();

  if (bpused == 0) return -1; // ярлык: если breakpoint не используется
  for (byte i=0; i < MAXBREAK*2; i++)
    if (bp[i].waddr == waddr && bp[i].used) return i;
  return -1;
}


void gdbHandleBreakpointCommand(const byte *buff)
{
  unsigned long byteflashaddr, sz;
  byte len;

  measureRam();

  if (targetOffline()) return;

  len = parseHex(buff + 3, &byteflashaddr);
  parseHex(buff + 3 + len + 1, &sz);
  
  /* тип прерывания */
  switch (buff[1]) {
  case '0':                           /* программная точка останова */
    if (buff[0] == 'Z') {
      if (gdbInsertBreakpoint(byteflashaddr >> 1)) 
	gdbSendReply("OK");
      else
	gdbSendReply("E03");
      return;
    } else {
      gdbRemoveBreakpoint(byteflashaddr >> 1);
      gdbSendReply("OK");
    }
    return;
  default:
    gdbSendReply("");
    break;
  }
}

/* вставляем bp, flash адрес в words */
bool gdbInsertBreakpoint(unsigned int waddr)
{
  int i,j;

  measureRam();

  // проверьте наличие дубликатов
  i = gdbFindBreakpoint(waddr);
  if (i >= 0)
    if (bp[i].active)  // это BP, установленное дважды, его можно игнорировать!
      return true;
  
  // если мы попытаемся установить слишком много BP, вернем false
  if (bpcnt == maxbreak) {
    return false;
  }

  // если бп уже есть, но не активен, то активируем
  i = gdbFindBreakpoint(waddr);
  if (i >= 0) { // existing bp
    bp[i].active = true;
    bpcnt++;
    // New recycled BP: waddr*2
    if (bp[i].inflash) { /* flash */ }
    /*       " / now active:" bpcnt      */
    return true;
  }
  // находим свободный слот (должен быть там, даже если есть MAXBREAK неактивных BP)
  for (i=0; i < MAXBREAK*2; i++) {
    if (!bp[i].used) {
      bp[i].used = true;
      bp[i].waddr = waddr;
      bp[i].active = true;
      bp[i].inflash = false;
      if (hwbp == 0xFFFF) { // аппаратный BP не используется
	bp[i].hw = true;
	hwbp = waddr;
      } else {              // мы крадем его у других bp, так как самым последним должен быть hwbp
	j = gdbFindBreakpoint(hwbp);
	if (j >= 0 && bp[j].hw) {
	  // Stealing HWBP from other BP: bp[j].waddr*2
	  bp[j].hw = false;
	  bp[i].hw = true;
	  hwbp = waddr;
	} else reportFatalError(HWBP_ASSIGNMENT_INCONSISTENT_FATAL, false);
      }
      bpcnt++;
      bpused++;
      // New BP: waddr*2;  " / now active:" bpcnt;
      if (bp[i].hw) { /*implemented as a HW BP*/ }
      return true;
    }
  }
  reportFatalError(NO_FREE_SLOT_FATAL, false);
  return false;
}

// деактивируем BP
void gdbRemoveBreakpoint(unsigned int waddr)
{
  int i;

  measureRam();

  i = gdbFindBreakpoint(waddr);
  if (i < 0) return;                  // могло произойти, если было попытано установить слишком много BP
  if (!bp[i].active) return;          // неактивно, может произойти из-за дублирования BP
  bp[i].active = false;
  bpcnt--;
  // BP removed: waddr*2;  now active: bpcnt
}

// после перезапуска проходим по таблице
// и очистка, сделав все BP неактивными,
// подсчитываем использованные и наконец вызываем update
void gdbCleanupBreakpointTable(void)
{
  int i;

  for (i=0; i < MAXBREAK*2; i++) {
    bp[i].active = false;
    if (bp[i].used) bpused++;
  }
  gdbUpdateBreakpoints(false); // теперь удалите все точки останова
}

// GDB хочет видеть 32 8-битных регистра (r00 - r31),
// 8-битный SREG, 16-битный SP и 32-битный PC,
// младшие байты перед старшими, поскольку AVR имеет прямой порядок байтов.
void gdbReadRegisters(void)
{
  byte a;
  unsigned int b;
  char c;
  unsigned int pc = (unsigned long)ctx.wpc << 1;	/* преобразуем адрес слова в адрес байта, используемый GDB */
  byte i = 0;

  a = 32;	/* в цикле отправьте R0 через R31 */
  b = (unsigned int) &(ctx.regs);
  
  do {
    c = *(char*)b++;
    buf[i++] = nib2hex((c >> 4) & 0xf);
    buf[i++] = nib2hex((c >> 0) & 0xf);
    
  } while (--a > 0);
  
  /* отправляем SREG как 32-й регистр */
  buf[i++] = nib2hex((ctx.sreg >> 4) & 0xf);
  buf[i++] = nib2hex((ctx.sreg >> 0) & 0xf);
  
  /* отправляем SP как регистр 33 */
  buf[i++] = nib2hex((ctx.sp >> 4)  & 0xf);
  buf[i++] = nib2hex((ctx.sp >> 0)  & 0xf);
  buf[i++] = nib2hex((ctx.sp >> 12) & 0xf);
  buf[i++] = nib2hex((ctx.sp >> 8)  & 0xf);
  
  /* отправляем ПК как 34-й регистр
    GDB хранит PC в 32-битном значении.
    GDB думает, что ПК — это байты во флэш-памяти, а не слова. */
  buf[i++] = nib2hex((pc >> 4)  & 0xf);
  buf[i++] = nib2hex((pc >> 0)  & 0xf);
  buf[i++] = nib2hex((pc >> 12) & 0xf);
  buf[i++] = nib2hex((pc >> 8)  & 0xf);
  buf[i++] = '0'; /* GDB хочет 32-битное значение, отправьте 0 */
  buf[i++] = '0'; /* GDB хочет 32-битное значение, отправьте 0 */
  buf[i++] = '0'; /* GDB хочет 32-битное значение, отправьте 0 */
  buf[i++] = '0'; /* GDB хочет 32-битное значение, отправьте 0 */
  
  buffill = i;
  gdbSendBuff(buf, buffill);
  
}

// устанавливаем все регистры со значениями, заданными GDB
void gdbWriteRegisters(const byte *buff)
{
  byte a;
  unsigned int pc;
  a = 32;	/* в цикле получаем от R0 до R31 */
  byte *ptr = &(ctx.regs[0]);

  measureRam();

  do {
    *ptr  = hex2nib(*buff++) << 4;
    *ptr |= hex2nib(*buff++);
  } while (--a > 0);
  
  /* получаем SREG как регистр 32 */
  ctx.sreg = hex2nib(*buff++) << 4;
  ctx.sreg |= hex2nib(*buff++);
  
  /* получаем SP как регистр 33 */
  ctx.sp  = hex2nib(*buff++) << 4;
  ctx.sp |= hex2nib(*buff++);
  ctx.sp |= hex2nib(*buff++) << 12;
  ctx.sp |= hex2nib(*buff++) << 8;
  /* получаем PC как регистр 34
    GDB хранит PC в 32-битном значении.
    GDB думает, что PC — это байты во флэш-памяти, а не слова. */
  pc  = hex2nib(*buff++) << 4;
  pc |= hex2nib(*buff++);
  pc |= hex2nib(*buff++) << 12;
  pc |= hex2nib(*buff++) << 8;
  pc |= (unsigned long)hex2nib(*buff++) << 20;
  pc |= (unsigned long)hex2nib(*buff++) << 16;
  pc |= (unsigned long)hex2nib(*buff++) << 28;
  pc |= (unsigned long)hex2nib(*buff++) << 24;
  ctx.wpc = pc >> 1;	/* отбрасываем младший бит; PC адресует слова */
  gdbSendReply("OK");
}

// считываем часть памяти и отправляем в нее GDB
void gdbReadMemory(const byte *buff)
{
  unsigned long addr;
  unsigned long sz, flag;
  byte i, b;

  measureRam();

  buff += parseHex(buff, &addr);
  /* пропускать 'xxx,' */
  parseHex(buff + 1, &sz);
  
  if (sz > MAXMEMBUF || sz*2 > MAXBUF) { // этого не должно происходить, потому что мы требовали, чтобы длина пакета была меньше
    gdbSendReply("E05");
    reportFatalError(PACKET_LEN_FATAL, false); // Packet length too large
    return;
  }

  if (addr == 0xFFFFFFFF) {
    buf[0] = nib2hex(fatalerror >> 4);
    buf[1] = nib2hex(fatalerror & 0x0f);
    gdbSendBuff(buf, 2);
    return;
  }

  if (targetOffline()) {
    gdbSendReply("E01");
    return;
  }

  flag = addr & MEM_SPACE_MASK;
  addr &= ~MEM_SPACE_MASK;
  if (flag == SRAM_OFFSET) targetReadSram(addr, membuf, sz);
  else if (flag == FLASH_OFFSET) {
    targetReadFlash(addr, membuf, sz);
    gdbHideBREAKs(addr, membuf, sz);
  } else if (flag == EEPROM_OFFSET) targetReadEeprom(addr, membuf, sz);
  else {
    gdbSendReply("E05");
    return;
  }
  for (i = 0; i < sz; ++i) {
    b = membuf[i];
    buf[i*2 + 0] = nib2hex(b >> 4);
    buf[i*2 + 1] = nib2hex(b & 0xf);
  }
  buffill = sz * 2;
  gdbSendBuff(buf, buffill);
}

// скрыть инструкции BREAK, которых там не должно быть (т. е. тех, которые еще не удалены)
void gdbHideBREAKs(unsigned int startaddr, byte membuf[], int size)
{
  int bpix;
  unsigned long addr;

  measureRam();

  for (addr = startaddr; addr < startaddr+size; addr++) {
    if ((addr & 1) && membuf[addr-startaddr] == 0x95) { // нечетный адрес и совпадение со старшим битом BREAK
      bpix = gdbFindBreakpoint((addr-1)/2);
      if (bpix >= 0 && bp[bpix].inflash)        // всегда скрывать:  && !bp[bpix].active) 
	membuf[addr-startaddr] = bp[bpix].opcode>>8;  // замена MSB в opcode
    }
    if (((addr&1) == 0) && membuf[addr-startaddr] == 0x98) { // тот же свмый адрес и совпадает с LSB для BREAK
      bpix = gdbFindBreakpoint(addr/2);
      if (bpix >= 0 && bp[bpix].inflash)        // всегда скрывать:  && !bp[bpix].active)
	membuf[addr-startaddr] = bp[bpix].opcode&0xFF;
    }
  }
}

// запись в память
void gdbWriteMemory(const byte *buff, bool binary)
{
  unsigned long sz, flag, addr,  i;
  long memsz;

  measureRam();

  if (targetOffline()) {
    gdbSendReply("E01");
    return;
  }

  buff += parseHex(buff, &addr);
  /* пропустить 'xxx,' */
  buff += parseHex(buff + 1, &sz);
  /* пропустить , и : разделители */
  buff += 2;

  // конвертируем в двоичные данные, удаляя escape-символы
  if (sz > MAXMEMBUF) { // этого не должно произойти, потому что мы требовали, чтобы длина пакета была меньше
    gdbSendReply("E05");
    reportFatalError(PACKET_LEN_FATAL, false);
    return;
  }
  if (binary) {
    memsz = gdbBin2Mem(buff, membuf, sz);
    if (memsz < 0) { 
      gdbSendReply("E05");
      reportFatalError(NEG_SIZE_FATAL, false);
      return;
    }
    sz = (unsigned long)memsz;
  } else {
    for ( i = 0; i < sz; ++i) {
      membuf[i]  = hex2nib(*buff++) << 4;
      membuf[i] |= hex2nib(*buff++);
    }
  }
    
  flag = addr & MEM_SPACE_MASK;
  addr &= ~MEM_SPACE_MASK;
  switch (flag) {
  case SRAM_OFFSET:
    if (addr+sz > mcu.ramsz+mcu.rambase) {
      gdbSendReply("E01"); 
      return;
    }
    targetWriteSram(addr, membuf, sz);
    break;
  case FLASH_OFFSET:
    if (addr+sz > mcu.flashsz) {// addr = addr, sz = sz, flashsz = mcu.flashsz
      gdbSendReply("E01"); 
      return;
    }
    setSysState(LOAD_STATE);
    targetWriteFlash(addr, membuf, sz);
    break;
  case EEPROM_OFFSET:
    if (addr+sz > mcu.eepromsz) {
      gdbSendReply("E01"); 
      return;
    }
    targetWriteEeprom(addr, membuf, sz);
    break;
  default:
    gdbSendReply("E05"); 
    reportFatalError(WRONG_MEM_FATAL, false);
    return;
  }
  gdbSendReply("OK");
}

// Преобразуем двоичный поток из BUF в память.
// Gdb экранирует $, # и escape-символ (0x7d).
// COUNT — общее количество байтов, которые нужно прочитать
int gdbBin2Mem(const byte *buf, byte *mem, int count) {
  int i, num = 0;
  byte  escape;

  measureRam();
  for (i = 0; i < count; i++) {
    /* Проверяем наличие экранированных символов. Будь параноиком и
       отменять экранирование только тех символов, которые следует экранировать. */
    escape = 0;
    if (*buf == 0x7d) {
      switch (*(buf + 1)) {
      case 0x03: /* # */
      case 0x04: /* $ */
      case 0x5d: /* escape */
      case 0x0a: /* - в документе сказано, что его следует избегать только в случае ответа, 
                    но avr-gdb его экранирует в любом случае */

	buf++;
	escape = 0x20;
	break;
      default:
	return -1; // сигнализируем об ошибке, если было экранировано что-то, чего не должно быть
	break;
      }
    }
    *mem = *buf++ | escape;
    mem++;
    num++;
  }
  return num;
}

// проверяем, подключено ли
bool targetOffline(void)
{
  measureRam();
  if (ctx.state == CONN_STATE || ctx.state == RUN_STATE || ctx.state == LOAD_STATE) return false;
  return true;
}


/****************** некоторые функции ввода-вывода GDB *************/
// очищаем входной буфер
void flushInput()
{
  while (Serial.available()) Serial.read(); 
}

// отправляем байт хосту
inline void gdbSendByte(byte b)
{
  Serial.write(b);
}

// блокируем чтение байта с хоста
inline byte gdbReadByte(void)
{
  while (!Serial.available());
  return Serial.read();
} 

void gdbSendReply(const char *reply)
{
  measureRam();

  buffill = strlen(reply);
  if (buffill > (MAXBUF - 4))
    buffill = MAXBUF - 4;

  memcpy(buf, reply, buffill);
  gdbSendBuff(buf, buffill);
}

void gdbSendSignal(byte signo)
{
  char buf[4];
  buf[0] = 'S';
  buf[1] = nib2hex((signo >> 4) & 0x0F);
  buf[2] = nib2hex(signo & 0x0F);
  buf[3] = '\0';
  gdbSendReply(buf);
}

void gdbSendBuff(const byte *buff, int sz)
{
  measureRam();

  byte sum = 0;
  gdbSendByte('$');
  while ( sz-- > 0)
    {
      gdbSendByte(*buff);
      sum += *buff;
      buff++;
    }
  gdbSendByte('#');
  gdbSendByte(nib2hex((sum >> 4) & 0xf));
  gdbSendByte(nib2hex(sum & 0xf));
}


void gdbSendPSTR(const char pstr[])
{
  byte sum = 0;
  byte c;
  int i = 0;
  
  gdbSendByte('$');
  do {
    c = pgm_read_byte(&pstr[i++]);
    if (c) {
      gdbSendByte(c);
      sum += c;
    }
  } while (c);
  gdbSendByte('#');
  gdbSendByte(nib2hex((sum >> 4) & 0xf));
  gdbSendByte(nib2hex(sum & 0xf));
}

void gdbState2Buf(byte signo)
{
  unsigned long wpc = (unsigned long)ctx.wpc << 1;

  /* поток всегда равен 1 */
  /* Реальный отправленный пакет, например. T0520:82;21:fb08;22:021b0000;поток:1;
      05 - ответ на последний сигнал
      Числа 20, 21,... — это номер регистра в шестнадцатеричном формате в том же порядке, что и в регистрах чтения.
      20 (32 декабря) = СРЕГ
      21 (33 дек) = СП
      22 (34 дес.) = ПК (байтовый адрес)
  */
  measureRam();

  memcpy_P(buf,
	   PSTR("TXX20:XX;21:XXXX;22:XXXXXXXX;thread:1;"),
	   38);
  buffill = 38;
  
  /* signo */
  buf[1] = nib2hex((signo >> 4)  & 0xf);
  buf[2] = nib2hex(signo & 0xf);
  
  /* sreg */
  buf[6] = nib2hex((ctx.sreg >> 4)  & 0xf);
  buf[7] = nib2hex(ctx.sreg & 0xf);
  
  /* sp */
  buf[12] = nib2hex((ctx.sp >> 4)  & 0xf);
  buf[13] = nib2hex((ctx.sp >> 0)  & 0xf);
  buf[14] = nib2hex((ctx.sp >> 12) & 0xf);
  buf[15] = nib2hex((ctx.sp >> 8)  & 0xf);
  
  /* pc */
  buf[20] = nib2hex((wpc >> 4)  & 0xf);
  buf[21] = nib2hex((wpc >> 0)  & 0xf);
  buf[22] = nib2hex((wpc >> 12) & 0xf);
  buf[23] = nib2hex((wpc >> 8)  & 0xf);
  buf[24] = '0';
  buf[25] = nib2hex((wpc >> 16) & 0xf);
  buf[26] = '0'; /* GDB хочет 32-битное значение, отправьте 0 */
  buf[27] = '0'; /* GDB хочет 32-битное значение, отправьте 0 */
}

void gdbSendState(byte signo)
{
  targetSaveRegisters();
  if (!targetOffline()) setSysState(CONN_STATE);
  switch (signo) {
  case SIGHUP:
    gdbDebugMessagePSTR(PSTR("Connection to target lost"),-1);
    setSysState(NOTCONN_STATE);
    break;
  case SIGILL:
    gdbDebugMessagePSTR(PSTR("Illegal instruction"),-1);
    break;
  case SIGABRT:
    gdbDebugMessagePSTR(PSTR("***Fatal internal debugger error: "),fatalerror);
    setSysState(ERROR_STATE);
    break;
  }
  gdbState2Buf(signo);
  gdbSendBuff(buf, buffill);
  lastsignal = signo;
}


// ответ на команду монитора
// используем строку из PROGMEN
// если последний аргумент < -1, то использовать его как индекс в массиве имен MCU (индекс: abs(num)-1)
void gdbReplyMessagePSTR(const char pstr[], long num) 
{
  gdbMessage(pstr, num, false, true);
}


// ответ на команду монитора
// используем строку из массива символов
void gdbReplyMessage(const char str[])
{
  gdbMessage(str, -1, false, false);       // str
}

// отправляем сообщение, которое может увидеть пользователь, если последний аргумент положительный, то отправляем номер
// если последний аргумент < -1, то использовать его как индекс в массиве имен MCU (индекс: abs(num)-1)
void gdbDebugMessagePSTR(const char pstr[],long num) 
{
  gdbMessage(pstr, num, true, true);
  
}

// отправляем сообщение с префиксом «O» или без него
void gdbMessage(const char pstr[],long num, bool oprefix, bool progmem)
{
  int i = 0, j = 0, c;
  byte numbuf[10];
  const char *str;

  // pstr, progmem
  if (oprefix) buf[i++] = 'O';
  do {
    c = (progmem ? pgm_read_byte(&pstr[j++]) : pstr[j++]);
    if (c) { // char
      if (i+4 >= MAXBUF) continue;
      buf[i++] = nib2hex((c >> 4) & 0xf);
      buf[i++] = nib2hex((c >> 0) & 0xf);
    }
  } while (c);
  if (num >= 0) {
    convNum(numbuf,num);
    j = 0;
    while (numbuf[j] != '\0') j++;
    while (j-- > 0 ) {
      if (i+4 >= MAXBUF) continue;       // (char)numbuf[j]
      buf[i++] = nib2hex((numbuf[j] >> 4) & 0xf);
      buf[i++] = nib2hex((numbuf[j] >> 0) & 0xf);
    }
  } else if (num == -2 ) { // распечатать имя MCU
    str = mcu.name;
    do {
      c = pgm_read_byte(str++);
      if (c) {
	if (i+4 >= MAXBUF) continue; // (char)c
	buf[i++] = nib2hex((c >> 4) & 0xf);
	buf[i++] = nib2hex((c >> 0) & 0xf);
      }
    } while (c );
  }
  buf[i++] = '0';
  buf[i++] = 'A';
  buf[i] = 0;
  gdbSendBuff(buf, i);
}


/****************** целевые функции *************/

// Попытка подключения к debugWIRE, выдав условие прерывания.
// Устанавливаем структуру mcu, если она еще не установлена.
// Возвращаем true в случае успеха.
bool targetDWConnect(void) {
  unsigned int sig;

  // Проверяем, требуется ли выход из режима debugWIRE
  if (doBreak()) {
    sig = DWgetChipId();    // targetConnect: doBreak done
    if (mcu.sig == 0) setMcuAttr(sig); // Устанавливаем структуру mcu, если она еще не установлена
    return true; // Возвращаем true в случае успешного подключения к debugWIRE
  }
  return false; // Возвращаем false, если не удалось выполнить условие прерывания
}

// Попытка установить соединение с debugWIRE и запрограммировать предохранитель DWEN.
// Возвращает:
//  1, если мы находимся в режиме debugWIRE и подключены
//  0, если нам нужно выключить и включить питание
// -1, если мы не можем подключиться
// -2, если тип MCU неизвестен
// -3, если установлены биты блокировки
int targetISPConnect(void) {
  unsigned int sig;
  int result;

  // Пытаемся войти в режим программирования
  if (!enterProgramMode()) return -1;

  // Получаем идентификатор чипа
  sig = ispGetChipId();
  if (sig == 0) { // Если не удалось прочитать сигнатуру
    result = -1;
  } else if (!setMcuAttr(sig)) { // Если не удалось установить атрибуты МК
    result = -2;
  } else if (ispLocked()) { // Если установлены биты блокировки
    result = -3;
  } else if (ispProgramFuse(true, mcu.dwenfuse, 0)) { // Программируем предохранитель DWEN
    result = 0;
  } else {
    result = -1;
  }

  // Покидаем режим программирования
  leaveProgramMode();

  // Возвращаем результат
  return result;
}

// Выключаем режим debugWIRE
bool targetStop(void) {
  int ret = targetSetFuses(DWEN); // Устанавливаем предохранитель DWEN
  leaveProgramMode(); // Покидаем режим программирования
  Serial2.end(); // Закрываем последовательный порт
  return (ret == 1); // Возвращаем результат
}

// устанавливаем фьюзы/очищаем память, возвращаем
//  1 - в случае успеха
// -1 - если не можем войти в режим программирования или сигнатура не читается
// -2 - если тип МК неизвестен
// -3 - программирование не удалось
int targetSetFuses(Fuses fuse)
{
  unsigned int sig;
  bool succ;

  measureRam();
  if (fuse == CkXtal && mcu.xtalosc == 0) return -4; // этот чип не поддерживает XTAL в качестве источника тактовой частоты
  if (fuse == CkSlow && mcu.slowosc == 0) return -5; // этот чип не может работать на частоте 128 кГц
  if (doBreak()) {
    sendCommand((const byte[]) {0x06}, 1); // выйти из режима debugWIRE
  }
  if (!enterProgramMode()) return -1;
  sig = ispGetChipId();
  if (sig == 0) {
    leaveProgramMode();
    return -1;
  }
  if (!setMcuAttr(sig)) {
    leaveProgramMode();
    return -2;
  }
  // теперь мы находимся в режиме ISP и знаем, с каким процессором имеем дело
  switch (fuse) {
  case CkDiv1: succ = ispProgramFuse(false, mcu.ckdiv8, mcu.ckdiv8); break;
  case CkDiv8: succ = ispProgramFuse(false, mcu.ckdiv8, 0); break;
  case CkRc:   succ = ispProgramFuse(false, mcu.ckmsk, mcu.rcosc); break;
  case CkExt: succ = ispProgramFuse(false, mcu.ckmsk, mcu.extosc); break;
  case CkXtal: succ = ispProgramFuse(false, mcu.ckmsk, mcu.xtalosc); break;
  case CkSlow: succ = ispProgramFuse(false, mcu.ckmsk, mcu.slowosc); break;
  case Erase: succ = ispEraseFlash(); break;
  case DWEN: succ = ispProgramFuse(true, mcu.dwenfuse, mcu.dwenfuse); break;
  default: succ = false;
  }
  return (succ ? 1 : -3);
}

// Читаем одну флэш-страницу с базовым адресом «addr» в глобальный буфер «page»,
// делаем это только в том случае, если страница еще не была прочитана,
// помним, что страница прочитана и буфер действителен
void targetReadFlashPage(unsigned int addr) {
  // Чтение флэш-страницы, начиная с адреса: addr
  if (addr != (addr & ~((unsigned int)(mcu.targetpgsz - 1)))) {
    // Ошибка адреса страницы при чтении
    reportFatalError(READ_PAGE_ADDR_FATAL, false); // Сообщаем об ошибке
    return;
  }
  if (!validpg || (lastpg != addr)) {
    targetReadFlash(addr, page, mcu.targetpgsz); // Читаем флэш-память и записываем в буфер page
    lastpg = addr; // Запоминаем адрес последней страницы
    validpg = true; // Устанавливаем флаг валидности страницы в true
  } else {
    // Используем кэшированную страницу lastpg
  }
}

// Читаем одно слово из флэш-памяти (адрес должен быть четным!)
unsigned int targetReadFlashWord(unsigned int addr) {
  byte temp[2];
  if (addr & 1) reportFatalError(FLASH_READ_WRONG_ADDR_FATAL, false); // Проверяем четность адреса
  DWreadFlash(addr, temp, 2); // Читаем из флэш-памяти
  return temp[0] + ((unsigned int)(temp[1]) << 8); // Возвращаем прочитанное слово
}

// Читаем часть флэш-памяти в буфер, на который указывает *mem'
// Не проверять кэшированные страницы и т.д.
void targetReadFlash(unsigned int addr, byte *mem, unsigned int len) {
  DWreadFlash(addr, mem, len); // Читаем флэш-память и записываем в буфер mem
}

// Читаем часть SRAM в буфер, на который указывает *mem
void targetReadSram(unsigned int addr, byte *mem, unsigned int len) {
  DWreadSramBytes(addr, mem, len); // Читаем SRAM и записываем в буфер mem
}

// Читаем часть EEPROM
void targetReadEeprom(unsigned int addr, byte *mem, unsigned int len) {
  for (unsigned int i = 0; i < len; i++) {
    mem[i] = DWreadEepromByte(addr++); // Читаем из EEPROM и записываем в буфер mem
  }
}

// Записываем флэш-страницу из буфера 'newpage'
// - проверяем, есть ли данные уже на этой флэш-странице; если да, ничего не делаем
// - проверяем, можно ли обойтись простой перезаписью, если не стереть страницу
// - пишем страницу и запоминаем содержимое страницы в буфере 'page'
// - если MCU использует операцию стирания 4 страниц, то выполняем 4 цикла загрузки/программирования для 4 подстраниц
void targetWriteFlashPage(unsigned int addr) {
  byte subpage;

  measureRam(); // Измерение использования оперативной памяти

  // Проверяем, правильно ли указан адрес страницы
  if (addr != (addr & ~(mcu.targetpgsz - 1))) {
    // Ошибка адреса страницы при записи
    reportFatalError(WRITE_PAGE_ADDR_FATAL, false); // Сообщаем об ошибке
    return;
  }

  DWreenableRWW(); // Разрешаем чтение и запись флэш-памяти

  // Читаем содержимое старой страницы
  targetReadFlashPage(addr);

  // Проверяем, изменилось ли что-то
  if (memcmp(newpage, page, mcu.targetpgsz) == 0) {
    // Страница не изменилась
    return;
  }

  // Проверяем, нужно ли нам стирать страницу
  bool dirty = false;
  for (byte i = 0; i < mcu.targetpgsz; i++) {
    if (~page[i] & newpage[i]) {
      dirty = true;
      break;
    }
  }

  validpg = false; // Сбрасываем флаг валидности страницы

  // Стираем страницу, если она грязная
  if (dirty) {
    DWeraseFlashPage(addr); // Стираем страницу
  }

  DWreenableRWW(); // Разрешаем чтение и запись флэш-памяти

  // Проверяем, не пуста ли новая страница
  memset(page, 0xFF, mcu.targetpgsz); // Заполняем буфер page FF-байтами
  if (memcmp(newpage, page, mcu.targetpgsz) == 0) {
    // Нечего писать
    validpg = true; // Устанавливаем флаг валидности страницы в true
    return;
  }

  // Занимаемся программированием; для 4-страничных MCU стираем 4 подстраницы
  for (subpage = 0; subpage < 1 + (3 * mcu.erase4pg); subpage++) {
    // Загружаем буфер страницы с данными из newpage
    DWloadFlashPageBuffer(addr + (subpage * mcu.pagesz), &newpage[subpage * mcu.pagesz]);
    // Программируем страницу
    DWprogramFlashPage(addr + subpage * mcu.pagesz);
    DWreenableRWW(); // Разрешаем чтение и запись флэш-памяти
  }

  // Запоминаем последнюю запрограммированную страницу
  memcpy(page, newpage, mcu.targetpgsz);
  validpg = true; // Устанавливаем флаг валидности страницы в true
  lastpg = addr; // Запоминаем адрес последней страницы
}

// Записываем некоторый фрагмент данных для флэш-памяти ленивым способом:
// - если длина пуста, записываем активную страницу во flash и возвращаемся
// - если новый байт, который нужно записать, выходит за границы текущей страницы, записываем страницу перед всем остальным
// - если это первый новый байт страницы, извлекаем эту страницу
// - вставляем новый байт на страницу
void targetWriteFlash(unsigned int addr, byte *mem, unsigned int len) {
  unsigned int ix;
  unsigned int newaddr;

  measureRam(); // Измерение использования оперативной памяти

  // Если длина пуста, записываем активную страницу во flash и возвращаемся
  if (len == 0) {
    if (!flashidle) targetWriteFlashPage(flashpageaddr); // Если флаг "запись во flash" не установлен, записываем текущую страницу во flash
    flashidle = true; // Устанавливаем флаг "запись во flash" в true, так как нет новых данных для записи
    return;
  }

  // Цикл по каждому байту данных для записи
  for (ix = 0; ix < len; ix++) {
    newaddr = addr + ix; // Вычисляем адрес нового байта данных

    // Если новый адрес выходит за границы текущей страницы и флаг "запись во flash" не установлен, записываем страницу перед всем остальным
    if ((newaddr < flashpageaddr || newaddr > flashpageaddr + mcu.targetpgsz - 1) && !flashidle) {
      targetWriteFlashPage(flashpageaddr); // Записываем текущую страницу во flash
      flashidle = true; // Устанавливаем флаг "запись во flash" в true, так как текущая страница записана
    }

    // Если флаг "запись во flash" установлен (или это первый новый байт страницы), извлекаем эту страницу
    if (flashidle) {
      flashpageaddr = newaddr & ~(mcu.targetpgsz - 1); // Вычисляем адрес начала страницы для записи
      targetReadFlashPage(flashpageaddr); // Читаем страницу из flash
      memcpy(newpage, page, mcu.targetpgsz); // Копируем страницу в буфер newpage
      flashidle = false; // Устанавливаем флаг "запись во flash" в false, так как страница готова к записи
    }

    newpage[newaddr - flashpageaddr] = mem[ix]; // Записываем новый байт на страницу
  }
}

// Сбрасываем все байты, которые еще нужно записать во флэш-память
inline void targetFlushFlashProg(void) {
  targetWriteFlash(0, membuf, 0);           // Вызываем функцию записи во флэш-память, передавая начальный адрес 0, буфер membuf и длину 0
}

// Записываем кусок SRAM
void targetWriteSram(unsigned int addr, byte *mem, unsigned int len) {
  measureRam();                             // Измерение использования оперативной памяти

  // Цикл записи каждого байта из mem в SRAM по адресу addr
  for (unsigned int i = 0; i < len; i++) {
    DWwriteSramByte(addr + i, mem[i]);      // Вызываем функцию записи одного байта в SRAM
  }
}

// Записываем кусок EEPROM
void targetWriteEeprom(unsigned int addr, byte *mem, unsigned int len) {
  measureRam();                             // Измерение использования оперативной памяти

  // Цикл записи каждого байта из mem в EEPROM по адресу addr
  for (unsigned int i = 0; i < len; i++) {
    DWwriteEepromByte(addr + i, mem[i]);    // Вызываем функцию записи одного байта в EEPROM
  }
}

// Инициализируем регистры (после RESET)
void targetInitRegisters(void) {
  byte a;
  a = 32;                                   // Значение, которое будем записывать в регистры R0-R31

  byte *ptr = &(ctx.regs[31]);              // Указатель на последний регистр R31

  measureRam();                             // Измерение использования оперативной памяти

  // Цикл для заполнения регистров R0-R31 значениями от 32 до 1
  do {
    *ptr-- = a;                             // Записываем значение a в текущий регистр, затем уменьшаем указатель
  } while (--a > 0);                        // Повторяем до тех пор, пока a не станет равным 0

  ctx.sreg = 0;                             // Устанавливаем регистр состояния (SREG) в 0
  ctx.wpc = 0;                              // Устанавливаем счетчик команд (PC) в 0
  ctx.sp = 0x1234;                          // Устанавливаем указатель стека (SP) в значение 0x1234
  ctx.saved = true;                         // Устанавливаем флаг сохранения регистров в true
}

// Прочитать все регистры из цели и сохранить их
void targetSaveRegisters(void) {
  measureRam();                             // Измерение использования оперативной памяти

  if (ctx.saved) return;                    // Если регистры уже сохранены, выходим из функции, чтобы не загружать их заново
  ctx.wpc = DWgetWPc(true);                 // Получение текущего значения счетчика команд (PC), учитывая возможное смещение инструкций
  DWreadRegisters(&ctx.regs[0]);            // Чтение всех общих регистров общего назначения (GP registers)
  ctx.sreg = DWreadIOreg(0x3F);             // Чтение регистра состояния (SREG)
  ctx.sp = DWreadIOreg(0x3D);               // Чтение указателя стека (SP)
  if (mcu.ramsz + mcu.rambase >= 256) ctx.sp |= DWreadIOreg(0x3E) << 8; // Если объем доступной оперативной памяти больше 256 байт, читаем второй байт SP
  ctx.saved = true;                         // Устанавливаем флаг сохранения регистров в true
}

// восстановить все регистры на цели (прежде чем продолжить выполнение)
void targetRestoreRegisters(void)
{
  measureRam();

  if (!ctx.saved) return;                   // если состояние не сохранено, не восстанавливать!
  DWwriteIOreg(0x3D, (ctx.sp&0xFF));
  if (mcu.ramsz > 256) DWwriteIOreg(0x3E, (ctx.sp>>8)&0xFF);
  DWwriteIOreg(0x3F, ctx.sreg);
  DWwriteRegisters(&ctx.regs[0]);
  DWsetWPc(ctx.wpc);                        // нужно делать в последнюю очередь!
  ctx.saved = false;                        // теперь мы можем сохранить их снова и быть уверенными, что получим правильные значения
}

// отправляем прерывание, чтобы остановить асинхронное выполнение
void targetBreak(void)
{
  measureRam();
//  sendBreak(); // отправляем прерывание
}

// начинаем выполнять
void targetContinue(void)
{
  measureRam();

  // Continue at (byte adress) = ctx.wpc*2
  if (hwbp != 0xFFFF) {
    sendCommand((const byte []) { 0x61 }, 1);
    DWsetWBp(hwbp);
  } else {
    sendCommand((const byte []) { 0x60 }, 1);
  }
  DWsetWPc(ctx.wpc);
  sendCommand((const byte []) { 0x30 }, 1);
}

// делаем один шаг
void targetStep(void)
{
  measureRam();

  // Single step at (byte address) = ctx.wpc*2
  byte cmd[] = {0x60, 0xD0, (byte)(ctx.wpc>>8), (byte)(ctx.wpc), 0x31};
  sendCommand(cmd, sizeof(cmd));
}

// Сброс микроконтроллера (MCU)
bool targetReset(void) {
  unsigned long timeout = 100000;           // Время ожидания сброса, в микросекундах

  sendCommand((const byte[]) {0x07}, 1);    // Отправляем команду сброса на линию передачи данных
  delay(10);                                // Задержка 10 миллисекунд

  // Ожидание завершения сброса или истечения тайм-аута
  while (digitalRead(DWLINE) && timeout) timeout--; // Пока линия DWLINE активна и тайм-аут не истек
  delay(1);                                 // Задержка 1 миллисекунда

  ctx.bps = 0;                              // Установка скорости передачи данных в ноль, чтобы принудительно установить новую скорость после сброса

  // Проверка успешности калибровки после сброса
  if (expectUCalibrate()) {
    // Сброс успешен
    return true;
  } else {
    // Сброс не удался
    reportFatalError(RESET_FAILED_FATAL, true); // Фатальная ошибка сброса
    return false;
  }
}

// Проверка на неправильные операционные коды (opcodes)
// Основано на: http://lyons42.com/AVR/Opcodes/AVRAllOpcodes.html
bool targetIllegalOpcode(unsigned int opcode) {
  byte lsb = opcode & 0xFF; // Младший байт операционного кода
  byte msb = (opcode & 0xFF00) >> 8; // Старший байт операционного кода

  measureRam(); // Измерение использования оперативной памяти

  switch (msb) {
  case 0x00: // nop (бездействие)
    return lsb != 0; // Возвращаем true, если младший байт не равен 0
  case 0x02: // muls (умножение со знаком)
  case 0x03: // mulsu/fmuls
    return !mcu.avreplus; // Возвращаем true, если не поддерживается умножение со знаком
  case 0x90:
  case 0x91: // lds, ld, lpm, elpm
    // Проверка на неправильные операции загрузки данных и программы из памяти
    if ((lsb & 0x0F) == 0x3 || (lsb & 0x0F) == 0x6 || (lsb & 0x0F) == 0x7 ||
	(lsb & 0x0F) == 0x8 || (lsb & 0x0F) == 0xB) return true; // Неправильные операции + elpm
    if (opcode == 0x91E1 || opcode == 0x91E2 || opcode == 0x91F1 || opcode == 0x91F2 ||
	opcode == 0x91E5 || opcode == 0x91F5 ||
	opcode == 0x91C9 || opcode == 0x91CA || opcode == 0x91D9 || opcode == 0x91DA ||
	opcode == 0x91AD || opcode == 0x91AE || opcode == 0x91BD || opcode == 0x91BE)
      return true; // Неопределенное поведение для операций ld и lpm с инкрементом
    return false;
  case 0x92:
  case 0x93:  // sts, st, push
    // Проверка на неправильные операции сохранения данных
    if (((lsb & 0xF) >= 0x3 && (lsb & 0xF) <= 0x8) || ((lsb & 0xF) == 0xB)) return true;
    if (opcode == 0x93E1 || opcode == 0x93E2 || opcode == 0x93F1 || opcode == 0x93F2 ||
	opcode == 0x93C9 || opcode == 0x93CA || opcode == 0x93D9 || opcode == 0x93DA ||
	opcode == 0x93AD || opcode == 0x93AE || opcode == 0x93BD || opcode == 0x93BE)
      return true; // Неопределенное поведение для операции st с инкрементом
    return false;
  case 0x94:
  case 0x95: // ALU, ijmp, icall, ret, reti, jmp, call, des, ...
    // Проверка на неправильные операции АЛУ и переходы
    if (opcode == 0x9409 || opcode == 0x9509) return false; //ijmp + icall
    if (opcode == 0x9508 || opcode == 0x9518 || opcode == 0x9588 || opcode == 0x95A8 ||
	opcode == 0x95C8 || opcode == 0x95E8) return false; // ret, reti, sleep, wdr, lpm, spm
    if ((lsb & 0xF) == 0x4 || (lsb & 0xF) == 0x9 || (lsb & 0xF) == 0xB) return true;
    if ((lsb & 0xF) == 0x8 && msb == 0x95) return true; // Неправильные операции + break + spm z+
    break;
  case 0x9c:
  case 0x9d:
  case 0x9e:
  case 0x9f: // mul
    return !mcu.avreplus; // Возвращаем true, если не поддерживается умножение
  default:
    if (((msb & 0xF8) == 0xF8) && ((lsb & 0xF) >= 8)) return true; // Неправильные операции
    return false;
  }

  // Для ATtiny, у которых CALL и JMP не нужны/не допускаются
  if (mcu.flashsz <= 8192)
    if ((opcode & 0x0FE0E) == 0x940C || (opcode & 0x0FE0E) == 0x940E)  // jmp, call
      return true;

  return false;
}

/****************** специальные функции debugWIRE *************/

// Отправка паузы на линию RESET, проверка реакции и выполнение калибровки
bool doBreak() {
  measureRam();                             // Измерение использования оперативной памяти

  // doBreak
  pinMode(DWLINE, INPUT);                   // Установка пина DWLINE в режим ввода
  delay(10);                                // Задержка 10 миллисекунд
  ctx.bps = 0;                              // Сброс скорости передачи данных (забыть о предыдущем подключении)
//  sendBreak();                              // Отправка сигнала паузы (break)

  // Проверка успешной калибровки после отправки паузы
  if (!expectUCalibrate()) {
    // Нет ответа после отправки сигнала паузы
    return false;
  }
  return true;                              // Успешное подключение с скоростью передачи данных bps = ctx.bps
}

// Повторная калибровка, ожидание 0x55, установка максимальной скорости передачи данных, т.е.
// умножаем скорость не более чем на 16 до 250 кбод - при условии, что у нас есть другая скорость, чем до
// возвращаем false, если синхронизация не удалась
bool expectUCalibrate(void) {
  int8_t speed; // Переменная для хранения настройки скорости
  unsigned long newbps; // Новая скорость передачи данных

  measureRam(); // Измерение использования оперативной памяти
  //blockIRQ(); // Блокировка прерываний
  newbps = calibrate(); // Ожидание 0x55 и калибровка скорости передачи данных

  // Проверка, является ли новая скорость слишком медленной
  if (newbps < 10) {
    ctx.bps = 0; // Сброс скорости
    //unblockIRQ(); // Разблокировка прерываний
    return false; // Слишком медленно
  }

  // Проверка, требуется ли изменение скорости, и если нет - возврат
  if ((100 * (abs((long)ctx.bps - (long)newbps))) / newbps <= 1)  {
    // Изменений не требуется: возврат
    //unblockIRQ(); // Разблокировка прерываний
    return true;
  }

  // Установка новой скорости и вычисление настройки скорости
  Serial2.begin(newbps);
  for (speed = maxspeedexp; speed > 0; speed--) {
    if ((newbps << speed) <= speedlimit) break;
  }

  // Установка настройки скорости
#if CONSTDWSPEED == 0
  DWsetSpeed(speed);
  ctx.bps = calibrate(); // Повторная калибровка
  //unblockIRQ(); // Разблокировка прерываний
  // Проверка, является ли вторая калибровка слишком медленной
  if (ctx.bps < 100) {
    // Вторая калибровка слишком медленна!
    return false; // Слишком медленно
  }
#else
  ctx.bps = newbps;
#endif

  // Установка скорости после калибровки
  Serial2.begin(ctx.bps);
  return true;
}

// Ожидание последовательности: сначала 0x00, затем 0x55, затем калибровка
bool expectBreakAndU(void)
{
  unsigned long timeout = 100000;           // Грубое время ожидания в 100-200 мс
  byte cc;                                  // Переменная для хранения полученного байта

  // Ожидайте, что устройство отправит байт со значением 0x00
  while (!Serial2.available() && timeout != 0) timeout--;
  if (timeout == 0) {
    // Тайм-аут в функции expectBreakAndU
    return false;
  }

  // Считайте байт и проверьте, равен ли он 0x00
  if ((cc = Serial2.read()) != 0) {
    // Ожидалось значение 0x00, но получено другое: cc
    return false;
  }

  // Если получено 0x00, продолжайте к ожиданию UCalibrate
  return expectUCalibrate();
}

// отправить команду
void sendCommand(const uint8_t *buf, uint8_t len)
{
  measureRam();

  Serial.flush();                           // подождем, пока все будет записано, чтобы избежать прерываний
  Serial2.write(buf, len);
}

// дождаться ответа и сохранить в buf
unsigned int getResponse (unsigned int expected) {
  measureRam();
  return getResponse(&buf[0], expected);
}

// Ожидание ответа от устройства и сохранение его в указанной области данных
unsigned int getResponse (byte *data, unsigned int expected) {
  unsigned int idx = 0;                            // Индекс для записи данных
  unsigned long timeout = 0;                       // Переменная для отслеживания тайм-аута

  measureRam();                                    // Измерение использования оперативной памяти

  if (overflow())                                  // Проверка на переполнение буфера
    reportFatalError(INPUT_OVERLFOW_FATAL, true);  // Обработка фатальной ошибки при переполнении

  do {
    if (Serial2.available()) {                     // Проверка наличия данных для чтения из порта
      data[idx++] = Serial2.read();                // Сохранение прочитанных данных и увеличение индекса
      timeout = 0;                                 // Сброс таймера тайм-аута
      if (expected > 0 && idx == expected) {       // Если ожидается определенное количество данных и они все получены
        return expected;                           // Возвращаем количество полученных данных
      }
    }
  } while (timeout++ < 20000);                     // Повторяем, пока не истечет тайм-аут или не будут получены все ожидаемые данные

  if (expected > 0) {
    // Если тайм-аут и ожидаются данные, генерируем сообщение об ошибке
    // Timeout: received: idx;  expected: expected
  }
  return idx;                                      // Возвращаем количество полученных данных
}

// Посылка команды и ожидание ответа, который должен быть словом (2 байта), начиная со старшего байта
unsigned int getWordResponse (byte cmd) {
  unsigned int response;
  byte tmp[2];
  byte cmdstr[] =  { cmd };                 // Создание массива байт для передачи команды
  measureRam();                             // Измерение использования оперативной памяти

  DWflushInput();                           // Очистка входного буфера
  //blockIRQ();                             // Блокировка прерываний (при необходимости)
  sendCommand(cmdstr, 1);                   // Отправка команды
  response = getResponse(&tmp[0], 2);       // Получение ответа (2 байта)
  //unblockIRQ();                           // Разблокировка прерываний (при необходимости)
  if (response != 2) reportFatalError(DW_TIMEOUT_FATAL,true); // Обработка ошибок, если ответ не получен
  return ((unsigned int) tmp[0] << 8) + tmp[1]; // Возвращение полученного слова (со старшим и младшим байтами)
}

// Установка альтернативной скорости связи
void DWsetSpeed(byte spix)
{
  byte speedcmdstr[1] = { pgm_read_byte(&speedcmd[spix]) }; // Создание массива байт для передачи команды скорости
  //Send speed cmd: speedcmdstr[0]
  sendCommand(speedcmdstr, 1); // Отправка команды установки скорости
}

// Функции, используемые для чтения регистров чтения и записи, SRAM и флэш-памяти, используют инструкции «in reg,addr» 
// и «out addr,reg». для передачи данных по debugWIRE через регистр DWDR. Однако, поскольку расположение регистра DWDR может 
// различаться в зависимости от устройства. для устройства необходимые инструкции «in» и «out» необходимо создавать динамически, 
// используя следующие 4 функции:
// 
//         -- --                            In:  1011 0aar rrrr aaaa
//      D2 B4 02 23 xx   - in r0,DWDR (xx)       1011 0100 0000 0010  a=100010(22), r=00000(00) // How it's used
//         -- --                            Out: 1011 1aar rrrr aaaa
//      D2 BC 02 23 <xx> - out DWDR,r0           1011 1100 0000 0010  a=100010(22), r=00000(00) // How it's used
//
// Примечание: 0xD2 устанавливает следующие два байта в качестве инструкции, которую затем выполняет 0x23. Итак, в первом примере 
// последовательность D2 B4 02 23 xx копирует значение xx в регистр r0 через регистр DWDR. Второй пример делает обратное и возвращает 
// значение в r0 как <xx>, отправив его в регистр DWDR.

// Создаем старший байт кода операции для инструкции "out addr, reg"
byte outHigh (byte add, byte reg) {
  // Формирование старшего байта кода операции для инструкции "out addr, reg":
  // Формат: 1011 1aar rrrr aaaa
  return 0xB8 + ((reg & 0x10) >> 4) + ((add & 0x30) >> 3);
}

// Создаем младший байт кода операции для инструкции "out addr, reg"
byte outLow (byte add, byte reg) {
  // Формирование младшего байта кода операции для инструкции "out addr, reg":
  // Формат: 1011 1aar rrrr aaaa
  return (reg << 4) + (add & 0x0F);
}

// Создаем старший байт кода операции для инструкции "in reg,addr"
byte inHigh  (byte add, byte reg) {
  // Формирование старшего байта кода операции для инструкции "in reg, addr":
  // Формат: 1011 0aar rrrr aaaa
  return 0xB0 + ((reg & 0x10) >> 4) + ((add & 0x30) >> 3);
}

// Создаем младший байт кода операции для инструкции "in reg,addr"
byte inLow  (byte add, byte reg) {
  // Формирование младшего байта кода операции для инструкции "in reg, addr":
  // Формат: 1011 0aar rrrr aaaa
  return (reg << 4) + (add & 0x0F);
}

// Запись всех регистров
void DWwriteRegisters(byte *regs)
{
  // Команды для записи всех регистров
  byte wrRegs[] = {0x66,                                // Установка для чтения/записи
                   0xD0, mcu.stuckat1byte, 0x00,        // Начальный регистр
                   0xD1, mcu.stuckat1byte, 0x20,        // Конечный регистр
                   0xC2, 0x05,                          // Запись регистров
                   0x20 };                              // Запуск
  measureRam();                                         // Измерение использования оперативной памяти
  sendCommand(wrRegs,  sizeof(wrRegs));                 // Отправка команды для записи всех регистров
  sendCommand(regs, 32);                                // Отправка значений регистров
}

// Установка значения в регистр <reg> путем создания и выполнения инструкции «in <reg>,DWDR» через регистр CMD_SET_INSTR
void DWwriteRegister (byte reg, byte val) {
  // Команды для установки значения в регистр через регистр DWDR
  byte wrReg[] = {0x64,                                                    // Установка для одного шага с использованием загруженной инструкции
                  0xD2, inHigh(mcu.dwdr, reg), inLow(mcu.dwdr, reg), 0x23, // Сборка инструкции "in reg,DWDR"
                  val};                                                    // Запись значения в регистр через DWDR
  measureRam();                                                            // Измерение использования оперативной памяти

  sendCommand(wrReg,  sizeof(wrReg));                                      // Отправка команды для установки значения в регистр
}

// Чтение всех регистров
void DWreadRegisters (byte *regs)
{
  int response;
  // Команды для чтения всех регистров
  byte rdRegs[] = {0x66,
                   0xD0, mcu.stuckat1byte, 0x00, // Начальный регистр
                   0xD1, mcu.stuckat1byte, 0x20, // Конечный регистр
                   0xC2, 0x01};                  // Чтение регистров
  measureRam();                                  // Измерение использования оперативной памяти
  DWflushInput();                                // Очистка входного буфера
  sendCommand(rdRegs,  sizeof(rdRegs));          // Отправка команды для чтения всех регистров
  //blockIRQ();                                  // Блокировка прерываний (при необходимости)
  sendCommand((const byte[]) {0x20}, 1);         // Запуск
  response = getResponse(regs, 32);              // Получение ответа на запрос
  //unblockIRQ();                                // Разблокировка прерываний (при необходимости)
  if (response != 32) reportFatalError(DW_READREG_FATAL,true); // Обработка ошибок, если ответ не получен
}

// Чтение регистра <reg> путем создания и выполнения инструкции «out DWDR,<reg>» через регистр CMD_SET_INSTR
byte DWreadRegister (byte reg) {
  int response;                                                         // Переменная для хранения ответа
  byte res = 0;                                                         // Переменная для хранения результата чтения
  // Команды для чтения регистра через регистр DWDR
  byte rdReg[] = {0x64,                                                 // Установка для одного шага с использованием загруженной инструкции
                  0xD2, outHigh(mcu.dwdr, reg), outLow(mcu.dwdr, reg)}; // Создание инструкции "out DWDR, reg"
  measureRam();                                                         // Измерение использования оперативной памяти
  DWflushInput();                                                       // Очистка входного буфера
  sendCommand(rdReg,  sizeof(rdReg));                                   // Отправка команды для чтения регистра
  //blockIRQ();                                                         // Блокировка прерываний (при необходимости)
  sendCommand((const byte[]) {0x23}, 1);                                // Запуск выполнения инструкции
  response = getResponse(&res,1);                                       // Получение ответа на запрос
  //unblockIRQ();                                                       // Разблокировка прерываний (при необходимости)
  if (response != 1) reportFatalError(DW_READREG_FATAL,true);           // Обработка ошибок, если ответ не получен
  return res;                                                           // Возврат результата чтения
}

// Запись одного байта в адресное пространство SRAM, используя значение на основе SRAM для <addr>, а не адрес ввода-вывода
void DWwriteSramByte (unsigned int addr, byte val) {
  // Команды для настройки записи в адресное пространство SRAM
  byte wrSram[] = {0x66,                                              // Установка для чтения/записи
                   0xD0, mcu.stuckat1byte, 0x1E,                      // Установка номера начального регистра (r30)
                   0xD1, mcu.stuckat1byte, 0x20,                      // Установка номера конечного регистра (r31) + 1
                   0xC2, 0x05,                                        // Установка повторяющегося копирования в регистры через DWDR
                   0x20,                                              // Запуск
		   (byte)(addr & 0xFF), (byte)(addr >> 8),            // Установка адреса в регистры r31:r30 (Z)
                   0xD0, mcu.stuckat1byte, 0x01,                      // Начальное значение регистра r30 для записи
                   0xD1, mcu.stuckat1byte, 0x03,                      // Установка количества повторений = 3
                   0xC2, 0x04,                                        // Установка имитированных инструкций "in r?,DWDR; st Z+,r?"
                   0x20,                                              // Запуск
                   val};                                              // Запись значения val
  measureRam();                                                       // Измерение использования оперативной памяти
  DWflushInput();                                                     // Очистка входного буфера
  sendCommand(wrSram, sizeof(wrSram));                                // Отправка команд для записи в адресное пространство SRAM
}

// Запись одного байта в регистр ввода-вывода (через R0)
void DWwriteIOreg (byte ioreg, byte val) {
  // Команды для настройки записи в регистр ввода-вывода
  byte wrIOreg[] = {0x64,                                             // Установка для одного шага с использованием загруженной инструкции
                    0xD2, inHigh(mcu.dwdr, 0), inLow(mcu.dwdr, 0), 0x23,  // Формирование инструкции "in reg,DWDR"
                    val,                                             // Загрузка значения val в регистр R0
                    0xD2, outHigh(ioreg, 0), outLow(ioreg, 0),       // Теперь сохранение значения из R0 в ioreg
                    0x23};                                           // Завершение
  measureRam();                                                      // Измерение использования оперативной памяти
  DWflushInput();                                                    // Очистка входного буфера
  sendCommand(wrIOreg, sizeof(wrIOreg));                             // Отправка команд для записи в регистр ввода-вывода
}

// Считывание одного байта из адресного пространства SRAM, используя значение на основе SRAM для <addr>, а не адрес ввода-вывода
byte DWreadSramByte (unsigned int addr) {
  byte res = 0;                                                       // Переменная для возвращаемого значения

  #if 1
    DWreadSramBytes(addr, &res, 1);                                   // Используется функция DWreadSramBytes для считывания одного байта
  #else
    unsigned int response;                                            // Переменная для хранения ответа
    // Команды для настройки чтения/записи в регистры
    byte rdSram[] = {0x66,                                            // Установка для чтения/записи
                     0xD0, mcu.stuckat1byte, 0x1E,                    // Установка номера начального регистра (r30)
                     0xD1, mcu.stuckat1byte, 0x20,                    // Установка номера конечного регистра (r31) + 1
                     0xC2, 0x05,                                      // Установка повторяющегося копирования в регистры через DWDR
                     0x20,                                            // Запуск
                     (byte)(addr & 0xFF), (byte)(addr >> 8),          // Установка адреса в регистры r31:r30 (Z)
                     0xD0, mcu.stuckat1byte, 0x00,                    // Начальное значение регистра r30 для чтения
                     0xD1, mcu.stuckat1byte, 0x02,                    // Установка количества повторений = 2
                     0xC2, 0x00};                                     // Установка имитированных инструкций "ld r?,Z+; out DWDR,r?"
    measureRam();                                                     // Измерение использования оперативной памяти
    DWflushInput();                                                   // Очистка входного буфера
    sendCommand(rdSram, sizeof(rdSram));                              // Отправка команд для настройки чтения/записи в регистры
    blockIRQ();                                                       // Блокировка прерывания
    sendCommand((const byte[]) {0x20}, 1);                            // Запуск
    response = getResponse(&res,1);                                   // Получение ответа
    unblockIRQ();                                                     // Разблокировка прерывания
    if (response != 1) reportFatalError(SRAM_READ_FATAL,true);        // Если ответ не равен 1, выдаётся фатальная ошибка
  #endif
  return res;                                                         // Возвращение значения
}

// Чтение одного байта из регистра ввода-вывода (через R0)
byte DWreadIOreg (byte ioreg) {
  unsigned int response;                                               // Переменная для хранения ответа
  byte res = 0;                                                        // Переменная для возвращаемого значения
  // Команды для настройки чтения из регистра ввода-вывода
  byte rdIOreg[] = {0x64,                                              // Установка для одного шага с использованием загруженной инструкции
                    0xD2, inHigh(ioreg, 0), inLow(ioreg, 0),           // Формирование инструкции "out DWDR, reg"
                    0x23,                                              // Запуск
                    0xD2, outHigh(mcu.dwdr, 0), outLow(mcu.dwdr, 0)};  // Формирование инструкции "out DWDR, 0"
  measureRam();                                                        // Измерение использования оперативной памяти
  DWflushInput();                                                      // Очистка входного буфера
  sendCommand(rdIOreg, sizeof(rdIOreg));                               // Отправка команд для настройки чтения из регистра ввода-вывода
  // Блокировка прерывания
  // sendCommand((const byte[]) {0x23}, 1);                            // Запуск
  response = getResponse(&res, 1); // Получение ответа
  // Разблокировка прерывания
  if (response != 1) reportFatalError(DW_READIOREG_FATAL, true);       // Если ответ не равен 1, выдаётся фатальная ошибка
  return res; // Возвращение значения
}

// Считывание <len> байт из адресного пространства SRAM в buf[], используя значение на основе SRAM для <addr>, а не адрес ввода-вывода
// Примечание: невозможно прочитать адреса, соответствующие r28-31 (регистры Y и Z), поскольку Z используется для передачи
// (не знаю, почему Y затерт)
void DWreadSramBytes (unsigned int addr, byte *mem, byte len) {
  unsigned int len2 = len * 2;                                      // Удвоенная длина для учета высоких и низких байтов адреса
  unsigned int rsp;                                                 // Переменная для хранения ответа
  // Команды для настройки чтения/записи в регистры
  byte rdSram[] = {0x66,                                            // Установка для чтения/записи
                   0xD0, mcu.stuckat1byte, 0x1E,                    // Установка номера начального регистра (r30)
                   0xD1, mcu.stuckat1byte, 0x20,                    // Установка номера конечного регистра (r31) + 1
                   0xC2, 0x05,                                      // Установка повторяющегося копирования в регистры через DWDR
                   0x20,                                            // Запуск
                   (byte)(addr & 0xFF), (byte)(addr >> 8),          // Установка адреса в регистры r31:r30 (Z)
                   0xD0, mcu.stuckat1byte, 0x00,                    // Начальное значение регистра r30 для чтения
                   0xD1, (byte)((len2 >> 8)+mcu.stuckat1byte), (byte)(len2 & 0xFF),  // Установка количества повторений = len * 2
                   0xC2, 0x00};                                     // Установка имитированных инструкций "ld r?,Z+; out DWDR,r?"
  measureRam();                                                     // Измерение использования оперативной памяти
  DWflushInput();                                                   // Очистка входного буфера
  sendCommand(rdSram, sizeof(rdSram));                              // Отправка команд для настройки чтения/записи в регистры
  // Блокировка прерывания
  // sendCommand((const byte[]) {0x20}, 1);                         // Запуск
  rsp = getResponse(mem, len); // Получение ответа
  // Разблокировка прерывания
  if (rsp != len) reportFatalError(SRAM_READ_FATAL,true);           // Если ответ не равен len, выдаётся фатальная ошибка
}

// Чтение одного байта из EEPROM по указанному адресу
byte DWreadEepromByte (unsigned int addr) {
  unsigned int response;                                                         // Переменная для хранения ответа
  byte retval;                                                                   // Переменная для возвращаемого значения
  // Команды для настройки чтения/записи в регистры
  byte setRegs[] = {0x66,                                                        // Установка для чтения/записи
                    0xD0, mcu.stuckat1byte, 0x1C,                                // Установка номера начального регистра (r28)
                    0xD1, mcu.stuckat1byte, 0x20,                                // Установка номера конечного регистра (r31) + 1
                    0xC2, 0x05,                                                  // Установка повторяющегося копирования в регистры через DWDR
                    0x20,                                                        // Запуск
                    0x01, 0x01, (byte)(addr & 0xFF), (byte)(addr >> 8)};         // Данные записываются в регистры r28-r31
  // Команды для чтения из EEPROM
  byte doReadH[] = {0xD2, outHigh(mcu.eearh, 31), outLow(mcu.eearh, 31), 0x23};  // out EEARH,r31  EEARH = ah  EEPROM Address MSB
  byte doRead[]  = {0xD2, outHigh(mcu.eearl, 30), outLow(mcu.eearl, 30), 0x23,   // out EEARL,r30  EEARL = al  EEPROM Address LSB
                    0xD2, outHigh(mcu.eecr, 28), outLow(mcu.eecr, 28), 0x23,     // out EECR,r28   EERE = 01 (EEPROM Read Enable)
                    0xD2, inHigh(mcu.eedr, 29), inLow(mcu.eedr, 29), 0x23,       // in  r29,EEDR   Чтение данных из EEDR
                    0xD2, outHigh(mcu.dwdr, 29), outLow(mcu.dwdr, 29)};          // out DWDR,r29   Отправка данных обратно через регистр DWDR
  measureRam();                                                                  // Измерение использования оперативной памяти
  DWflushInput();                                                                // Очистка входного буфера
  sendCommand(setRegs, sizeof(setRegs));                                         // Отправка команд для настройки чтения/записи в регистры
  // Блокировка прерывания
  // sendCommand((const byte[]){0x64},1);                                        // Установка для одного шага с использованием загруженной инструкции
  // Если есть высокий байт регистра EEAR, установите его
  if (mcu.eearh) sendCommand(doReadH, sizeof(doReadH));
  sendCommand(doRead, sizeof(doRead));                                           // Установка остальных контрольных регистров и запрос
  sendCommand((const byte[]) {0x23}, 1);                                         // Запуск
  response = getResponse(&retval,1);                                             // Получение ответа
  // Разблокировка прерывания
  if (response != 1) reportFatalError(EEPROM_READ_FATAL,true);                   // Если ответ не равен 1, выдаётся фатальная ошибка
  return retval;                                                                 // Возвращение значения
}

// Запись одного байта в EEPROM по указанному адресу
void DWwriteEepromByte (unsigned int addr, byte val) {
  // Команды для настройки чтения/записи в регистры
  byte setRegs[] = {0x66,                                                         // Установка для чтения/записи
                    0xD0, mcu.stuckat1byte, 0x1C,                                 // Установка номера начального регистра (r30)
                    0xD1, mcu.stuckat1byte, 0x20,                                 // Установка номера конечного регистра (r31) + 1
                    0xC2, 0x05,                                                   // Установка повторяющегося копирования в регистры через DWDR
                    0x20,                                                         // Запуск
                    0x04, 0x02, (byte)(addr & 0xFF), (byte)(addr >> 8)};          // Данные записываются в регистры r28-r31
  // Команды для записи в EEPROM
  byte doWriteH[] ={0xD2, outHigh(mcu.eearh, 31), outLow(mcu.eearh, 31), 0x23};   // out EEARH,r31  EEARH = ah  EEPROM Address MSB
  byte doWrite[] = {0xD2, outHigh(mcu.eearl, 30), outLow(mcu.eearl, 30), 0x23,    // out EEARL,r30  EEARL = al  EEPROM Address LSB
                    0xD2, inHigh(mcu.dwdr, 30), inLow(mcu.dwdr, 30), 0x23,        // in  r30,DWDR   Получение данных для записи через DWDR
                    val,                                                          // Данные записываются в указанное место EEPROM
                    0xD2, outHigh(mcu.eedr, 30), outLow(mcu.eedr, 30), 0x23,      // out EEDR,r30   EEDR = data
                    0xD2, outHigh(mcu.eecr, 28), outLow(mcu.eecr, 28), 0x23,      // out EECR,r28   EECR = 04 (EEPROM Master Program Enable)
                    0xD2, outHigh(mcu.eecr, 29), outLow(mcu.eecr, 29), 0x23};     // out EECR,r29   EECR = 02 (EEPROM Program Enable)
  measureRam();                                                                   // Измерение использования оперативной памяти
  sendCommand(setRegs, sizeof(setRegs));                                          // Отправка команд для настройки чтения/записи в регистры
  // Если есть высокий байт регистра EEAR, установите его
  if (mcu.eearh) sendCommand(doWriteH, sizeof(doWriteH));
  sendCommand(doWrite, sizeof(doWrite));                                          // Отправка команд для записи в EEPROM
  delay(5);                                                                       // Ожидание завершения записи в EEPROM
}

// Чтение len байт из области флэш-памяти по адресу addr в буфер mem
void DWreadFlash(unsigned int addr, byte *mem, unsigned int len) {
  unsigned int rsp;                                         // Переменная для хранения ответа
  unsigned int lenx2 = len * 2;                             // Удвоенное значение len
  // Команды для чтения флэш-памяти
  byte rdFlash[] = {0x66,                                   // Установка для чтения/записи
                    0xD0, mcu.stuckat1byte, 0x1E,           // Установка номера начального регистра (r30)
                    0xD1, mcu.stuckat1byte, 0x20,           // Установка номера конечного регистра (r31) + 1
                    0xC2, 0x05,                             // Установка повторяющегося копирования в регистры через DWDR
                    0x20,                                   // Запуск
                    (byte)(addr & 0xFF), (byte)(addr >> 8), // r31:r30 (Z) = addr
                    0xD0, mcu.stuckat1byte, 0x00,           // Установка начала = 0
                    0xD1, (byte)((lenx2 >> 8)+mcu.stuckat1byte),(byte)(lenx2), // Установка конца = количеству повторений = sizeof(flashBuf) * 2
                    0xC2, 0x02};                            // Установка имитации инструкций "lpm r?,Z+; out DWDR,r?"
  measureRam();                                             // Измерение использования оперативной памяти
  DWflushInput();                                           // Очистка входного буфера
  sendCommand(rdFlash, sizeof(rdFlash));                    // Отправка команд для чтения флэш-памяти
  sendCommand((const byte[]) {0x20}, 1);                    // Запуск
  rsp = getResponse(mem, len);                              // Чтение len байт
  if (rsp != len) reportFatalError(FLASH_READ_FATAL,true);  // Если количество прочитанных байт не равно len, выдаётся фатальная ошибка
}

// Стирает всю страницу флэш-памяти
void DWeraseFlashPage(unsigned int addr) {
  byte timeout = 0;                            // Счетчик времени ожидания
  // Команды для стирания страницы
  byte eflash[] = { 0x64,                      // Однократное выполнение команды
                    0xD2,                      // Загрузка в регистр инструкций
                    outHigh(0x37, 29),         // Формирование "out SPMCSR, r29"
                    outLow(0x37, 29),
                    0x23,                      // Выполнение команды
                    0xD2, 0x95 , 0xE8 };       // Выполнение команды SPM
  measureRam();                                // Измерение использования оперативной памяти
  // Стирание: addr
  while (timeout < TIMEOUTMAX) {               // Пока не истекло максимальное время ожидания
    DWflushInput(); // Очистка входного буфера
    DWwriteRegister(30, addr & 0xFF); // Загрузка младшего байта адреса в регистр Z
    DWwriteRegister(31, addr >> 8  ); // Загрузка старшего байта адреса в регистр Z
    DWwriteRegister(29, 0x03); // Установка значения PGERS для SPMCSR
    // Если адрес загрузки установлен, установка значения PC на адрес загрузки
    if (mcu.bootaddr) DWsetWPc(mcu.bootaddr); // Для возможности доступа ко всей флэш-памяти
    // Отправка команды для стирания страницы
    sendCommand(eflash, sizeof(eflash));
    sendCommand((const byte[]) {0x33}, 1);    // Отправка команды
    if (expectBreakAndU()) break;             // Ожидание обратной связи
    delay(1600);                              // Задержка
    timeout++;                                // Увеличение счетчика времени ожидания
    timeoutcnt++;                             // Увеличение счетчика превышений времени ожидания
  }
  // Если истекло максимальное время ожидания, выдаётся фатальная ошибка
  if (timeout >= TIMEOUTMAX) reportFatalError(FLASH_ERASE_FATAL,true);
}
		    
// Перемещает страницу из временной памяти во флэш-память
void DWprogramFlashPage(unsigned int addr)
{
  bool succ; // Флаг успешного выполнения операции программирования
  byte timeout = 0; // Счетчик времени ожидания
  unsigned int wait = 10000; // Максимальное время ожидания
  // Команды для программирования страницы
  byte eprog[] = { 0x64, // Однократное выполнение команды
                   0xD2, // Загрузка в регистр инструкций
                   outHigh(0x37, 29), // Формирование "out SPMCSR, r29"
                   outLow(0x37, 29),
                   0x23,  // Выполнение команды
                   0xD2, 0x95 , 0xE8}; // Выполнение команды SPM

  // Программирование страницы флэш-памяти ...
  measureRam();
  flashcnt++; // Увеличение счетчика программированных страниц
  // Пока не истекло максимальное время ожидания
  while (timeout < TIMEOUTMAX) {
    wait = 1000;
    DWflushInput(); // Очистка входного буфера
    // Загрузка адреса страницы в регистры Z
    DWwriteRegister(30, addr & 0xFF); // Загрузка младшего байта адреса в регистр Z
    DWwriteRegister(31, addr >> 8  ); // Загрузка старшего байта адреса в регистр Z
    // Установка флага PGWRT в SPMCSR
    DWwriteRegister(29, 0x05); // Установка значения PGWRT для SPMCSR
    // Если адрес загрузки установлен, установка значения PC на адрес загрузки
    if (mcu.bootaddr) DWsetWPc(mcu.bootaddr); // Для возможности доступа ко всей флэш-памяти
    // Отправка команды для программирования страницы
    sendCommand(eprog, sizeof(eprog));

    sendCommand((const byte[]) {0x33}, 1); // Отправка команды
    succ = expectBreakAndU(); // Ожидание обратной связи

    // Если нет загрузчика и успешно выполнено программирование
    if (mcu.bootaddr && succ) {
      delay(100);
      // Ожидание завершения операций программирования
      while ((DWreadSPMCSR() & 0x1F) != 0 && --wait != 0) {
        delay(10); // Ожидание
      }
      succ = (wait != 0);
    }
    // Если успешно выполнено программирование, выход из цикла
    if (succ) break;
    delay(1); // Задержка
    timeout++; // Увеличение счетчика времени ожидания
    timeoutcnt++; // Увеличение счетчика превышений времени ожидания
  }
  // Если истекло максимальное время ожидания, выдаётся фатальная ошибка
  if (timeout >= TIMEOUTMAX) reportFatalError(FLASH_PROGRAM_FATAL,true);
  // ...завершено
}

// Загружает байты во временную память страницы Flash
void DWloadFlashPageBuffer(unsigned int addr, byte *mem)
{
  // Команды для загрузки байтов во временную память
  byte eload[] = { 0x64, 0xD2,
                   outHigh(0x37, 29),      // Формирование "out SPMCSR, r29"
                   outLow(0x37, 29),
                   0x23,                   // Выполнение команды
                   0xD2, 0x95, 0xE8, 0x23, // Команда SPM
                   0xD2, 0x96, 0x32, 0x23, // Команда addiw Z,2
  };

  // Загрузка страницы Flash ...
  measureRam();
  DWflushInput();                          // Очистка входного буфера

  // Загрузка адреса страницы в регистры Z
  DWwriteRegister(30, addr & 0xFF);        // Загрузка младшего байта адреса в регистр Z
  DWwriteRegister(31, addr >> 8  );        // Загрузка старшего байта адреса в регистр Z
  DWwriteRegister(29, 0x01);               // Установка значения SPMEN для SPMCSR

  byte ix = 0;
  while (ix < mcu.pagesz) {                // Пока не загружены все байты страницы
    DWwriteRegister(0, mem[ix++]);         // Загрузка следующего слова (2 байта) в регистры 0 и 1
    DWwriteRegister(1, mem[ix++]);

    if (mcu.bootaddr) DWsetWPc(mcu.bootaddr); // Если адрес загрузки установлен, установка значения PC на адрес загрузки
    sendCommand(eload, sizeof(eload));     // Отправка команды для загрузки данных во временную память
  }
  // ...завершено
}

// Включает возможность чтения при записи (RWW) для микроконтроллера
void DWreenableRWW(void)
{
  // Задержка для ожидания завершения операций записи
  unsigned int wait = 10000;

  // Команды для включения RWW
  byte errw[] = { 0x64, 0xD2,
                  outHigh(0x37, 29),       // Формирование "out SPMCSR, r29"
                  outLow(0x37, 29),
                  0x23,                    // Выполнение команды
                  0xD2, 0x95, 0xE8, 0x23 }; // Команда SPM

  // Измерение использования оперативной памяти
  measureRam();

  // Если адрес загрузки установлен
  if (mcu.bootaddr) {
    // Ожидание завершения операций записи в SPMCSR
    while ((DWreadSPMCSR() & 0x01) != 0 && --wait) {
      delay(10);    // Ожидание
    }

    // Если ожидание превышено, выдаётся фатальная ошибка
    if (wait == 0) {
      reportFatalError(REENABLERWW_FATAL, true);
      return;
    }

    // Установка значения PC на адрес загрузки
    DWsetWPc(mcu.bootaddr);

    // Запись значения RWWSRE в регистр 29 (SPMCSR)
    DWwriteRegister(29, 0x11); // Значение RWWSRE для SPMCSR

    // Отправка команды для включения RWW
    sendCommand(errw, sizeof(errw));
  }
}

// Считывает текущее значение регистра SPM Control and Status Register (SPMCSR)
byte DWreadSPMCSR(void)
{
  // Команды для настройки одиночного шага и загрузки регистра команд
  byte sc[] = { 0x64, 0xD2,                // настройка для одиночного шага и загрузки регистра команд
                inHigh(0x37, 30),          // формирование "in 30, SPMCSR"
                inLow(0x37, 30),
                0x23 };                    // выполнение команды
  measureRam();                            // Измерение использования оперативной памяти
  DWflushInput();                          // Очистка входного буфера
  sendCommand(sc, sizeof(sc));             // Отправка команды
  return DWreadRegister(30);               // Считывание значения регистра SPMCSR и его возврат
}

// Получает текущее значение регистра PC (счетчика программ) с возможностью коррекции
// Если параметр corrected установлен в true, функция корректирует значение регистра PC
unsigned int DWgetWPc(bool corrected) {
  unsigned int pc = getWordResponse(0xF0); // Получаем текущее значение регистра PC в формате слова (word response) по команде 0xF0
  if (corrected) {                         // Если нужна коррекция значения регистра PC
    pc &= ~(mcu.stuckat1byte << 8);        // Отключаем stuckat1byte из старшего байта значения PC
    pc--;                                  // Уменьшаем значение PC на 1 для коррекции
  }
  return pc;                               // Возвращаем полученное или скорректированное значение регистра PC
}


// Получает идентификационный номер микросхемы (chip signature)
unsigned int DWgetChipId() {
  unsigned int result = getWordResponse(0xF3); // Получаем ответ в формате слова (word response) от устройства по команде 0xF3
  return result;                               // Возвращаем полученный идентификационный номер микросхемы
}

// Устанавливает значение регистра PC (счетчика программ) в формате слова (word address), включая все биты (включая те, которые находятся в stuckat1byte)
void DWsetWPc(unsigned int wpcreg) {
  // Формируем команду установки значения регистра PC: 0xD0, старший байт wpcreg с учетом mcu.stuckat1byte, младший байт wpcreg
  byte cmd[] = {0xD0, (byte)((wpcreg >> 8) + mcu.stuckat1byte), (byte)(wpcreg & 0xFF)};
  sendCommand(cmd, sizeof(cmd));           // Отправляем команду установки значения регистра PC
}

// Устанавливает аппаратную точку останова
void DWsetWBp(unsigned int wbp) {
  // Формируем команду установки breakpoint: 0xD1, старший байт wbp с учетом mcu.stuckat1byte, младший байт wbp
  byte cmd[] = {0xD1, (byte)((wbp >> 8) + mcu.stuckat1byte), (byte)(wbp & 0xFF)};

  // Отправляем команду установки breakpoint
  sendCommand(cmd, sizeof(cmd));
}

// Выполняет инструкцию офлайн (может быть 2-байтной или 4-байтной)
void DWexecOffline(unsigned int opcode)
{
  // Формируем команду для выполнения офлайн: 0xD2, старший байт opcode, младший байт opcode, 0x23
  byte cmd[] = {0xD2, (byte) (opcode >> 8), (byte) (opcode&0xFF), 0x23};
  measureRam();                            // Измеряем использование оперативной памяти (RAM)
  sendCommand(cmd, sizeof(cmd));           // Отправляем команду для выполнения офлайн
}

// Очищает буфер Serial2 и возвращает последний прочитанный байт
byte DWflushInput(void)
{
  byte c = 0;                              // переменная для хранения прочитанного байта
  while (Serial2.available()) {            // Пока есть данные в буфере Serial2
    c = Serial2.read();                    // Читаем байт из Serial2 и сохраняем его в переменной c
  }
  // delayMicroseconds(1);                 // Задержка (возможно, не нужна)
  return c;                                // Возвращаем прочитанный байт
}

/***************************** не большой SPI программатор ********/

// Включает пины, используемые для SPI
void enableSpiPins() {
  // Включаем SPI...
  pinMode(DWLINE, OUTPUT);                 // Устанавливаем пин DWLINE в режим вывода
  digitalWrite(DWLINE, LOW);               // и устанавливаем его в LOW
  pinMode(SCLK1, OUTPUT);                  // Устанавливаем SCLK1 в режим вывода
  digitalWrite(SCLK1, LOW);                // и устанавливаем его в LOW
  pinMode(MOSI1, OUTPUT);                  // Устанавливаем MOSI1 в режим вывода
  digitalWrite(MOSI1, HIGH);               // и устанавливаем его в HIGH
  pinMode(MISO1, INPUT);                   // Устанавливаем MISO1 в режим входа
  digitalWrite(TISP, LOW);                 // Устанавливаем TISP в LOW для включения SPI
}

// Отключает пины, используемые для SPI
void disableSpiPins() {
  digitalWrite(TISP, HIGH);                // Устанавливаем TISP в HIGH для отключения SPI
  pinMode(SCLK1, INPUT);                   // Устанавливаем SCLK1 в режим входа
  digitalWrite(MOSI1, LOW);                // Отключаем подтягивающий резистор для MOSI1
  pinMode(MOSI1, INPUT);                   // Устанавливаем MOSI1 в режим входа
  pinMode(MISO1, INPUT);                   // Устанавливаем MISO1 в режим входа
}

// Передает байт через SPI и возвращает результат
byte ispTransfer(byte val) {
  measureRam();                            // Измеряет использование оперативной памяти (RAM)

  // Частота ISP теперь 12500
  // это должно быть достаточно медленно даже для
  // тактовой частоты МК 128 кГц
  for (byte ii = 0; ii < 8; ++ii) {
    digitalWrite(MOSI1, (val & 0x80) ? HIGH : LOW); // Отправляем бит последовательно через MOSI1
    digitalWrite(SCLK1, HIGH);             // Устанавливаем SCLK1 в HIGH, чтобы передать бит
    delay(200);                            // Задержка для стабилизации
    val = (val << 1) + digitalRead(MISO1); // Сдвигаем принятый бит влево и добавляем принятый бит
    digitalWrite(SCLK1, LOW);              // Устанавливаем SCLK1 в LOW для завершения передачи бита
    delay(200);                            // Задержка для стабилизации
  }
  return val;                              // Возвращаем принятый байт
}

// Отправляет последовательность байтов через SPI и возвращает последний полученный байт
byte ispSend(byte c1, byte c2, byte c3, byte c4, bool last) {
  byte res;
  ispTransfer(c1);                         // Отправляем первый байт
  ispTransfer(c2);                         // Отправляем второй байт
  res = ispTransfer(c3);                   // Отправляем третий байт и сохраняем результат
  if (last)
    res = ispTransfer(c4);                 // Если это последний байт, отправляем и возвращаем результат
  else
    ispTransfer(c4);                       // В противном случае просто отправляем последний байт
  return res;                              // Возвращаем результат
}

// Вход в режим программирования
bool enterProgramMode()
{
  byte timeout = 5;

  // Входим в режим программирования
//  enable(false);
  do {
    // Выполняем ...
    enableSpiPins();
    // Включены пины ...
    pinMode(DWLINE, INPUT);
    delay(30);                             // Короткий положительный импульс RESET длиной не менее 2 тактовых циклов
    pinMode(DWLINE, OUTPUT);
    delay(30);                             // Ждем как минимум 20 мс перед отправкой последовательности включения
    if (ispSend(0xAC, 0x53, 0x00, 0x00, false) == 0x53) break;
  } while (--timeout);

  if (timeout == 0) {
    leaveProgramMode();                    // ... неудачно
    return false;
  } else {
    // ... успешно
    delay(15);                             // Ждем после включения программирования - avrdude делает так!
    return true;
  }
}

// Выход из режима программирования
void leaveProgramMode()
{
  disableSpiPins();                        // Покидаем режим программирования
  delay(10);
  pinMode(DWLINE, INPUT);                  // Разрешаем МК работать или общаться через debugWIRE
//  enable(true);
}

// Получает идентификатор чипа через протокол ISP
unsigned int ispGetChipId()
{
  unsigned int id;

  if (ispSend(0x30, 0x00, 0x00, 0x00, true) != 0x1E) return 0; // Проверяем идентификационный байт
  id = ispSend(0x30, 0x00, 0x01, 0x00, true) << 8;             // Считываем старший байт ID
  id |= ispSend(0x30, 0x00, 0x02, 0x00, true);                 // Считываем младший байт ID

  // Возвращаем идентификатор чипа
  return id;
}

// Программирует фьюзы (регистры конфигурации) и/или высокие фьюзы микроконтроллера.
bool ispProgramFuse(bool high, byte fusemsk, byte fuseval)
{
  byte newfuse;
  byte lowfuse, highfuse, extfuse;
  bool succ = true;

  // Читаем текущие значения фьюзов
  lowfuse = ispSend(0x50, 0x00, 0x00, 0x00, true);   // Читаем низкие фьюзы
  highfuse = ispSend(0x58, 0x08, 0x00, 0x00, true);  // Читаем высокие фьюзы
  extfuse = ispSend(0x50, 0x08, 0x00, 0x00, true);   // Читаем расширенные фьюзы

  // Определяем, какие фьюзы будем программировать
  if (high) newfuse = highfuse;                     // Если high == true, выбираем высокие фьюзы
  else newfuse = lowfuse;                           // Иначе выбираем низкие фьюзы

  // Программируем выбранные фьюзы в соответствии с маской и значением
  newfuse = (newfuse & ~fusemsk) | (fuseval & fusemsk);

  // Обновляем значения фьюзов
  if (high) highfuse = newfuse;                     // Если high == true, обновляем высокие фьюзы
  else lowfuse = newfuse;                           // Иначе обновляем низкие фьюзы

  // Программируем фьюзы и проверяем успешность операции
  ispSend(0xAC, 0xA0, 0x00, lowfuse, true);         // Программируем низкие фьюзы
  delay(15);                                        // Ждем завершения операции записи
  succ &= (ispSend(0x50, 0x00, 0x00, 0x00, true) == lowfuse); // Проверяем, успешно ли записали низкие фьюзы

  ispSend(0xAC, 0xA4, 0x00, extfuse, true);         // Программируем расширенные фьюзы
  delay(15);                                        // Ждем завершения операции записи
  succ &= (ispSend(0x50, 0x08, 0x00, 0x00, true) == extfuse); // Проверяем, успешно ли записали расширенные фьюзы

  ispSend(0xAC, 0xA8, 0x00, highfuse, true);        // Программируем высокие фьюзы
  delay(15);                                        // Ждем завершения операции записи
  succ &= (ispSend(0x58, 0x08, 0x00, 0x00, true) == highfuse); // Проверяем, успешно ли записали высокие фьюзы

  return succ;                                      // Возвращаем результат программирования фьюзов
}

// Стирает флеш-память микроконтроллера с помощью программатора.
bool ispEraseFlash(void)
{
  ispSend(0xAC, 0x80, 0x00, 0x00, true);   // Отправляем команду для стирания флеш-памяти
  delay(20);                               // Ждем 20 миллисекунд для завершения операции стирания
  pinMode(DWLINE, INPUT);                  // Устанавливаем вывод DWLINE в режим ввода для создания короткого положительного импульса
  delay(1);                                // Ждем 1 миллисекунду
  pinMode(DWLINE, OUTPUT);                 // Устанавливаем вывод DWLINE обратно в режим вывода
  return true;                             // Возвращаем true для указания успешного завершения операции стирания
}

// Проверяет, заблокирован ли программатор ISP.
bool ispLocked()
{
  return (ispSend(0x58, 0x00, 0x00, 0x00, true) != 0xFF);
}

// Устанавливает атрибуты микроконтроллера на основе его идентификатора.
bool setMcuAttr(unsigned int id)
{
  int ix = 0;
  unsigned int sig;
  measureRam();                            // Устанавливаем стартовую точку для измерения доступной памяти.

  while ((sig = pgm_read_word(&mcu_info[ix].sig))) {
    if (sig == id) {                       // Найден нужный тип микроконтроллера
      // Структура MCU:
      mcu.sig = sig;
      mcu.ramsz = pgm_read_byte(&mcu_info[ix].ramsz_div_64)*64;
      mcu.rambase = (pgm_read_byte(&mcu_info[ix].rambase_low) ? 0x60 : 0x100);
      mcu.eepromsz = pgm_read_byte(&mcu_info[ix].eepromsz_div_64)*64;
      mcu.flashsz = pgm_read_byte(&mcu_info[ix].flashsz_div_1k)*1024;
      mcu.dwdr = pgm_read_byte(&mcu_info[ix].dwdr);
      mcu.pagesz = pgm_read_byte(&mcu_info[ix].pagesz_div_2)*2;
      mcu.erase4pg = pgm_read_byte(&mcu_info[ix].erase4pg);
      mcu.bootaddr = pgm_read_word(&mcu_info[ix].bootaddr);
      mcu.eecr =  pgm_read_byte(&mcu_info[ix].eecr);
      mcu.eearh =  pgm_read_byte(&mcu_info[ix].eearh);
      mcu.rcosc =  pgm_read_byte(&mcu_info[ix].rcosc);
      mcu.extosc =  pgm_read_byte(&mcu_info[ix].extosc);
      mcu.xtalosc =  pgm_read_byte(&mcu_info[ix].xtalosc);
      mcu.slowosc =  pgm_read_byte(&mcu_info[ix].slowosc);
      mcu.avreplus = pgm_read_byte(&mcu_info[ix].avreplus);
      mcu.name = (const char *)pgm_read_word(&mcu_info[ix].name); // !!!!!! mcu.name = (const char *)&mcu_info[ix].name;
      // Оставшиеся поля будут вычислены
      mcu.eearl = mcu.eecr + 2;
      mcu.eedr = mcu.eecr + 1;
      // Мы обрабатываем MCU с 4-страничным стиранием так, будто страницы были бы больше в 4 раза!
      if (mcu.erase4pg) mcu.targetpgsz = mcu.pagesz*4;
      else mcu.targetpgsz = mcu.pagesz;
      // dwen, chmsk, ckdiv8 идентичны почти для всех MCUs, поэтому мы обрабатываем исключения здесь
      mcu.dwenfuse = 0x40;
      mcu.ckmsk = 0x3F;
      mcu.ckdiv8 = 0x80;
      if (mcu.name == attiny13) {
        mcu.ckdiv8 = 0x10;
        mcu.dwenfuse = 0x08;
        mcu.ckmsk = 0x0F;
      } else if (mcu.name == attiny2313 || mcu.name == attiny4313) {
        mcu.dwenfuse = 0x80;
      }
      return true;
    }
    ix++;
  }
  // Не удалось определить тип MCU по SIG: id
  return false;
}

/*****************************  процедуры преобразования **************/
// Конвертирует значение из 4 бит в шестнадцатеричный символ.
char nib2hex(byte b)
{
  measureRam();                            // Устанавливаем стартовую точку для измерения доступной памяти.

  return((b) > 9 ? 'a' - 10 + b: '0' + b);
}

// Конвертирует шестнадцатеричный символ в значение из 4 бит.
byte hex2nib(char hex)
{
  measureRam();                            // Устанавливаем стартовую точку для измерения доступной памяти.

  hex = toupper(hex);
  return(hex >= '0' && hex <= '9' ? hex - '0' :
     (hex >= 'A' && hex <= 'F' ? hex - 'A' + 10 : 0xFF));
}

// Функция для парсинга строки в шестнадцатеричное число.
uint8_t parseHex(const uint8_t *buff, uint32_t *hex)
{
    uint8_t nib, len;

    measureRam();                          // Устанавливаем стартовую точку для измерения доступной памяти.

    for (*hex = 0, len = 0; (nib = hex2nib(buff[len])) != 0xFF; ++len)
        *hex = (*hex << 4) + nib;          // Преобразуем символы в шестнадцатеричное значение.

    return len;                            // Возвращаем количество успешно обработанных символов.
}

// Функция для конвертации числа в строку, записывая число задом наперёд
void convNum(byte numbuf[10], long num)
{
  int i = 0;

  if (num == 0) numbuf[i++] = '0';         // Если число равно нулю, записываем в буфер символ '0'

  while (num > 0) {                        // Пока число больше нуля
    numbuf[i++] = (num%10) + '0';          // Записываем в буфер последнюю цифру числа, прибавляя символ '0'
    num = num / 10;                        // Делим число на 10 для перехода к следующей цифре
  }
  numbuf[i] = '\0';                        // Добавляем завершающий символ конца строки
}


 





