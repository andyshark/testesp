//-------------------------------------------------------
// глобальные переменные
//-------------------------------------------------------
#ifndef __GLOBAL_VARS
#define __GLOBAL_VARS
 
#include <Arduino.h>
#include <string.h>
#include <ESP8266WiFi.h>                                                // Библиотека для создания Wi-Fi подключения (клиент или точка доступа)
#include <ESP8266WebServer.h>                                           // Библиотека для управления устройством по HTTP (например из браузера)
#include <LittleFS.h>                                                   // Библиотека для работы с файловой системой
#include <FTPServer.h>                                                  // Библиотека для работы с LittleFS по FTP
//#include <UniversalTelegramBot.h>
//#include <ArduinoJson.h>
#include <GDBStub.h>

#define Tube          0x13011974
#define bit_is_clear(var,bit) ((var & (1<<(bit))) ==0)
#define bit_is_set(var,bit) ((var & (1<<(bit))) !=0)

#define EEPROM_START_ADDR 0   // начальный адрес эмулируемой EEPROM
#define EEPROM_SIZE 512       // размер эмулируемой EEPROM

// смещение в EEPROM
#define eEECRC            (EEPROM_START_ADDR + 0)                     // контрольная сумма eeprom начиная с 4 адреса и до конца
#define eTxTube           (EEPROM_START_ADDR + sizeof(EECRC))         // труба TX
#define eRxTube1          (EEPROM_START_ADDR + sizeof(TxTube))        // труба 1 RX
#define eChannel          (EEPROM_START_ADDR + sizeof(RxTube1))       // канал
#define eAddress          (EEPROM_START_ADDR + sizeof(Channel))       // начальный адрес прибора
#define eCorrTermo        (EEPROM_START_ADDR + sizeof(Address))       // значение поправки датчика температуры кристалла
#define eConfig           (EEPROM_START_ADDR + sizeof(CorrTermo))     // конфигурация  
#define eslpDelay         (EEPROM_START_ADDR + sizeof(Config))        // время спячки ms
#define eCmpADC_1         (EEPROM_START_ADDR + sizeof(slpDelay))      // значение порога для ацп 1
#define eCmpADC_2         (EEPROM_START_ADDR + sizeof(CmpADC_1))      // значение порога для ацп 2
#define eMaddress         (EEPROM_START_ADDR + sizeof(CmpADC_2))      // адрес мастера  
#define eStrtAdrSlv       (EEPROM_START_ADDR + sizeof(Maddress))      // начальный адрес с которого в порядке возрастания идут адреса подчиненных модулей  
#define eNSlv             (EEPROM_START_ADDR + sizeof(StrtAdrSlv))    // количество подчиненных модулей протечки
#define eRxTube2          (EEPROM_START_ADDR + sizeof(NSlv))          // труба 2 RX
#define eTxTube2          (EEPROM_START_ADDR + sizeof(RxTube2))       // труба 2 TX
#define eSlTimeout        (EEPROM_START_ADDR + sizeof(TxTube2))       // таймаут слейвов
#define ewww_username     (EEPROM_START_ADDR + sizeof(SlTimeout))     // имя пользователя для доступа к содержимому странички
#define ewww_password     (EEPROM_START_ADDR + sizeof(www_username))  // пароль для доступа к содержимому странички
#define essidAP           (EEPROM_START_ADDR + sizeof(www_password))  // Название генерируемой точки доступа
#define essidAP_password  (EEPROM_START_ADDR + sizeof(ssidAP))        // Пароль для подключения к роутеру
#define essid             (EEPROM_START_ADDR + sizeof(ssidAP_password))// Название сети WIFI роутера
#define essid_password    (EEPROM_START_ADDR + sizeof(ssid))           // Пароль для генерируемой точки доступа
#define etimezone         (EEPROM_START_ADDR + sizeof(ssid_password))  // часовой пояс GTM
#define ebot_token        (EEPROM_START_ADDR + sizeof(timezone))       // токен бота
#define echat_ID          (EEPROM_START_ADDR + sizeof(bot_token))      // ID чата

// П И Н Ы
#if defined(ARDUINO_ESP8266_ESP12)
//#define LED_BUILTIN   2                // светодиод

// RF24 9 - 14
#define RF24CSNpin    0                  // радиомодуль CSN - Если установлен низкий уровень, то модуль отвечает на SPI команды. 
                                         // CSN более важный сигнал выбора, чем сигнал CE
//#define SCK         14                  // радиомодуль - тактирование шины SPI, до 10 МГц                                         
//#define MISO        12                  // радиомодуль - для передачи данных из устройства в микроконтроллер
//#define MOSI        13                  // радиомодуль - используется для передачи данных от микроконтроллера к устройству
#define RF24IRQ       4                  // радиомодуль IRQ - сигнал для запроса прерывания при отправке и получении пакета
#define RF24CEpin     16                 // радиомодуль CE - включение радиотракта микросхемы высоким уровнем
#endif

#if defined(ESP8266_WEMOS_D1MINI)
//#define LED_BUILTIN   2                // светодиод

// RF24 9 - 14
#define RF24CSNpin    0                  // радиомодуль CSN - Если установлен низкий уровень, то модуль отвечает на SPI команды. 
                                         // CSN более важный сигнал выбора, чем сигнал CE
//#define SCK         14                  // радиомодуль - тактирование шины SPI, до 10 МГц                                         
//#define MISO        12                  // радиомодуль - для передачи данных из устройства в микроконтроллер
//#define MOSI        13                  // радиомодуль - используется для передачи данных от микроконтроллера к устройству
#define RF24IRQ       4                  // радиомодуль IRQ - сигнал для запроса прерывания при отправке и получении пакета
#define RF24CEpin     16                 // радиомодуль CE - включение радиотракта микросхемы высоким уровнем

#endif

struct Pkt
{
  uint8_t DstAdr;          // адрес приемника и шифровальный ключ    
  uint8_t CMD;             // команда 
  uint8_t SrcAdr;          // адрес иточника
  uint8_t buf[8];          // буффер
  uint8_t Param1;          // дополнительный параметр
  uint8_t crc;             // кс
};

struct FarPkt
{
  uint8_t buf[8];           // буффер    
  uint32_t oldTime;        // время прихода последнего пакета
};

struct WM
{
 uint64_t      RxTube1;                  // труба
 uint64_t      TxTube1;                  // труба
 uint64_t      RxTube2;                  // труба
 uint64_t      TxTube2;                  // труба
 uint8_t       Address;                  // адрес прибора
 uint8_t       Channel;                  // канал
 uint8_t       Maddress;                 // адрес мастера
 uint32_t      slpDelay;                 // время спячки ms
 int           CorrTermo;                // значение поправки датчика температуры кристалла
 uint8_t       CmpADC_1;                 // значение порога для ацп 1
 uint8_t       CmpADC_2;                 // значение порога для ацп 2
 uint8_t       Config;                   // конфигурация
 uint8_t       StrtAdrSlv;               // начальный адрес с которого в порядке возрастания идут до 16 адресов подчиненных
 uint8_t       NSlv;                     // количество подчиненных модулей протечки
 uint32_t      SlTimeout;                // общий таймаут для любого слейва 
 uint32_t      TOpenClose;               // аварийное время переключения задвижки ms
};

const byte relay = LED_BUILTIN;         // Пин подключения сигнального контакта реле

extern uint8_t        Config;           // конфигурация
#define EnaIn         0                 // разрешить состояние входов
#define EnaAn         1                 // аналоговый измеритель
#define SlveRF        2                 // удаленные модули
#define onRouter      3                 // подключение через роутер
#define onStatic      4                 // подключение через роутер со статическим ip адресои
                                       
extern uint8_t        Alarm;            // биты для тревоги Alarm 
#define NotConnect    0                 // нет соединения 
#define InterZone1    1                 // контроль зоны 1
#define InterZone2    2                 // контроль зоны 2
#define InterZone3    3                 // контроль зоны 3
#define InterZone4    4                 // контроль зоны 4
#define InterZone5    5                 // контроль зоны 5
#define InterZone6    6                 // контроль зоны 6
#define InterZone7    7                 // контроль напряжения питания
                                                                                                                     
extern uint8_t        StatCool;         // управленние и наблюдение за задвижкой
extern uint8_t        StatHot;          // управленние и наблюдение за задвижкой
// биты в _status
// open               0                 // задание открыть
// close              1                 // задание открыть
// valve_open         2                 // ос открыто
// valve_close        3                 // ос закрыто
// valve_move         4                 // сейчас находится в движении
// valve_err_open     5                 // ошибка при попытке открыть
// valve_err_close    6                 // ошибка при попытке закрыть
// valve_err_power    7                 // ошибка питания
                                                                                                                     
extern Pkt            SLpkt;            // структкра пакета slave
extern FarPkt         FarSLpkt[4][17];  // массив для хранения пакетов от удаленных датчиков
extern uint8_t        Address;          // начальный адрес прибора
extern uint8_t        Maddress;         // адрес мастера
extern uint64_t       TxTube;           // труба TX
extern uint64_t       TxTube2;          // труба 2 TX
extern uint64_t       RxTube1;          // труба 1 RX
extern uint64_t       RxTube2;          // труба 2 RX
extern uint8_t        Channel;          // канал
extern uint8_t        Vbat;             // напряжение питания 
//extern uint8_t        ADC_1;            // значение ацп 1
//extern uint8_t        ADC_2;            // значение ацп 2
extern uint8_t        CmpADC_1;         // значение порога для ацп 1
extern uint8_t        CmpADC_2;         // значение порога для ацп 2
extern int            DTermo;           // значение датчика температуры кристалла
extern int            CorrTermo;        // значение поправки датчика температуры кристалла
extern uint32_t       EECRC;            // контрольная сумма eeprom начиная с 4 адреса и до конца
extern uint32_t       slpDelay;         // время спячки ms
extern uint8_t        AlmZoneMask;      // маска алармов
extern uint8_t        ExternAlm1;       // есть ошибка на внешнем модуле (номер бита 0..7 это номер слейва 0..7)
extern uint8_t        ExternAlm2;       // есть ошибка на внешнем модуле (номер бита 0..7 это номер слейва 8..15)
extern uint8_t        StrtAdrSlv;       // начальный адрес с которого в порядке возрастания идут адреса подчиненных модулей
extern uint8_t        NSlv;             // количество подчиненных модулей
extern uint32_t       SlTimeout;        // общий таймаут для любого слейва
extern char           www_username[17]; // Логин  для доступа к содержимому странички
extern char           www_password[17]; // Пароль для доступа к содержимому странички
extern char           ssidAP[17];       // Название генерируемой точки доступа
extern char           ssidAP_password[17];// Пароль для генерируемой точки доступа
extern char           ssid[17];         // Название сети WIFI роутера
extern char           ssid_password[17];// Пароль для подключения к роутеру
extern char           bot_token[81];    // токен бота
extern char           chat_ID[81];      // ID чата
extern ESP8266WebServer HTTP;           // Определяем объект и порт сервера для работы с HTTP
extern FTPServer     ftpSrv;            // Определяем объект для работы с модулем по FTP (для отладки HTML)
extern int           timezone;          // часовой пояс GTM
extern WM            setupWM;           // структура для настройки модуля протечки
#endif
