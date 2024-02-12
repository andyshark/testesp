#include <Arduino.h> 
// EEPROM
#include "EpromVar.h"                                                   // импортируем библиотеку
// RF24
#include "RF24Net.h"                                                    // подключение модуля протокола связи
#include "RF24.h"                                                       // подключение библиотеки NRF24L01
// мои
#include "global_var.h"                                                 // подключение экспорта глобальных переменных  
#include "HttpInit.h"

//-------------------------------------------------------
// прототипы функций
//-------------------------------------------------------
extern "C" void gdb_do_break();
IRAM_ATTR void gpio_Handler();

//-------------------------------------------------------
// переменные
//-------------------------------------------------------
//Если нужен Статический IP адрес, тогда раскомментировать эти строки, так же ещё в двух местах раскоментировать ниже в коде
IPAddress ip(192, 168, 1, 12);                                        //Статический IP-адрес, меняем на свой
IPAddress gateway(192, 168, 1, 1);                                    //IP-адрес роутера, меняем на свой
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer HTTP(80);                                              // Определяем объект и порт сервера для работы с HTTP
FTPServer ftpSrv(LittleFS);                                             // Определяем объект для работы с модулем по FTP (для отладки HTML)

uint8_t       Vbat;                     // напряжение питания в сотнях милливольт 3,3 вольт = 33
int           DTermo;                   // значение датчика температуры кристалла
//uint8_t       ADC_1;                    // значение ацп
//uint8_t       ADC_2;                    // значение ацп
uint8_t       CmpADC_1;                 // значение порога для ацп 1
uint8_t       CmpADC_2;                 // значение порога для ацп 2
uint8_t       Alarm;                    // внутренние тревоги
int           CorrTermo;                // значение поправки датчика температуры кристалла
Pkt           SLpkt;                    // структкра пакета slave
FarPkt        FarSLpkt[4][17];          // массив для хранения пакетов от удаленных датчиков
uint32_t      EECRC;                    // контрольная сумма eeprom начиная с 4 адреса и до конца                                 
uint8_t       Address;                  // начальный адрес прибора
uint8_t       Maddress;                 // адрес мастера
uint8_t       Config;                   // конфигурация
uint64_t      TxTube;                   // труба
uint64_t      TxTube2;                  // труба 2
uint64_t      RxTube1;                  // труба 1
uint64_t      RxTube2;                  // труба 2
uint8_t       Channel;                  // канал
uint32_t      slpDelay;                 // время спячки ms
uint8_t       AlmZoneMask;              // маска алармов
uint8_t       ExternAlm1;               // есть ошибка на внешнем модуле (номер бита 0..7 это номер слейва 0..7)
uint8_t       ExternAlm2;               // есть ошибка на внешнем модуле (номер бита 0..7 это номер слейва 8..15)
uint8_t       StatCool;                 // управленние и наблюдение за задвижкой
uint8_t       StatHot;                  // управленние и наблюдение за задвижкой
uint8_t       StrtAdrSlv;               // начальный адрес с которого в порядке возрастания идут адреса подчиненных модулей
uint8_t       NSlv;                     // количество подчиненных модулей протечки
uint32_t      SlTimeout;                // общий таймаут для любого слейва
uint8_t       EnaTranslateapkt;         // раазрешить трансляцию пакетов
char          www_username[17];         // Логин  для доступа к содержимому странички
char          www_password[17];         // Пароль для доступа к содержимому странички
char          ssidAP[17];               // Название генерируемой точки доступа
char          ssidAP_password[17];      // Пароль для генерируемой точки доступа
char          ssid[17];                 // Название сети WIFI роутера
char          ssid_password[17];        // Пароль для подключения к роутеру
char          bot_token[81];            // токен бота
char          chat_ID[81];              // ID чата
int           timezone;                 // часовой пояс GTM
WM            setupWM;                  // структура для настройки датчика протечки

//-------------------------------------------------------
// обработчик аппаратного прерывания GPIO
//-------------------------------------------------------
IRAM_ATTR void gpio_Handler() { // IRQ активный НИЗКИЙ уровень
 noInterrupts();
 digitalWrite(LED_BUILTIN, LOW);         // включаем светодиод
 whyIrq(); 
 digitalWrite(LED_BUILTIN, HIGH);        // выключаем светодиод
 interrupts();
} 

//-------------------------------------------------------
// настройка платы
//-------------------------------------------------------  
void setup() {
  //Serial.begin(115200);                   // Инициализируем вывод данных на COM порт
  gdbstub_init();

  pinMode(RF24IRQ, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);           // светодиод
  pinMode(RF24CSNpin, OUTPUT);  
  pinMode(RF24CEpin, OUTPUT);

  EEPROM.begin(EEPROM_SIZE); 
  EECRC = EEPROM_ulong_read(eEECRC);       // читаем кс
  if(EECRC !=  eeprom_onA4_crc())  {       // кс верная?
   TxTube = Tube;                          // нет, начальная TX труба 
   RxTube1 = Tube + 1;                     // начальная RX 1 труба
   RxTube2 = RxTube1 + 1;                  // начальная RX 2 труба 
   TxTube2 = RxTube2 + 1;                  // начальная TX 2 труба    
   Channel = 100;                          // канал
   Address = 255;                          // начальный адрес прибора
   CorrTermo = 0;                          // коррекция температуры кристалла
   Config = 255;                           // разрешено все
   slpDelay = 60000;                       // время спячки для слейвов 60 сек
   CmpADC_1 = 90;                          // порог аварии аналогового входа
   CmpADC_2 = 90;                          // порог аварии аналогового входа
   Maddress = 1;                           // адрес мастера (WI-FI модуль)
   StrtAdrSlv = 16;                        // адрес первого слейва
   NSlv = 0;                               // количество слейвов
   SlTimeout = 10000;                      // таймаут для слейва
   //-----------------------------------------------------------
   strcpy(www_username, "admin");          // Логин  для доступа к содержимому странички
   strcpy(www_password, "admin");          // Пароль для доступа к содержимому странички
   strcpy(ssidAP, "DiAnd");                // Название генерируемой точки доступа
   strcpy(ssidAP_password, "admin");       // Пароль для генерируемой точки доступа
   strcpy(ssid, "ksysha");                 // Название сети WIFI роутера
   strcpy(ssid_password, "natasha1975");   // Пароль для подключения к роутеру
   timezone = 5;                           // часовой пояс GTM
   bitClear(Config, onRouter);             // будем поднимать точку доступа
   //Store();
  } else {                                 // да, читаем начальные уставки из eeprom  
   TxTube = EEPROM_uint64_read(eTxTube);
   TxTube2 = EEPROM_uint64_read(eTxTube2); 
   RxTube1 = EEPROM_uint64_read(eRxTube1);
   RxTube2 = EEPROM_uint64_read(eRxTube2);
   Channel = EEPROM.read(eChannel);
   Address = EEPROM.read(eAddress);
   CorrTermo = EEPROM_int_read(eCorrTermo);
   Config =  EEPROM.read(eConfig); 
   slpDelay = EEPROM_ulong_read(eslpDelay); 
   CmpADC_1 = EEPROM.read(eCmpADC_1);
   CmpADC_2 = EEPROM.read(eCmpADC_2);
   Maddress = EEPROM.read(eMaddress);
   StrtAdrSlv = EEPROM.read(eStrtAdrSlv);
   SlTimeout = EEPROM.read(eSlTimeout); 
   NSlv = EEPROM.read(eNSlv); 
   //-----------------------------------------------------------   
   EEPROM_String_read(ewww_username, www_username);
   EEPROM_String_read(ewww_password, www_password);
   EEPROM_String_read(essidAP, ssidAP);
   EEPROM_String_read(essidAP_password, ssidAP_password);
   EEPROM_String_read(essid, ssid);
   EEPROM_String_read(essid_password, ssid_password);
   EEPROM_String_read(ebot_token, bot_token);
   EEPROM_String_read(echat_ID, chat_ID);
   timezone = EEPROM_int_read(etimezone);
  } 

  //Режим Wi-Fi
  if(bit_is_clear(Config, onRouter)) {                                  // флаг onRouter сброшен?
    WiFi.softAP(ssidAP);                                                // да, Создаём точку доступа 
  } else {                                                              // нет, идем через роутер
    WiFi.mode(WIFI_STA);                                                // выбираем режим работы
    WiFi.begin(ssid, ssid_password);                                    // работа с паролем
    if(bit_is_set(Config, onStatic))                                    // флаг onStatic установлен? 
      WiFi.config(ip, gateway, subnet);                                 // да, исполбзуем Статический IP адрес
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
    }
  }
  


  LfsFtpHttpInit();
  SetupRadio();                                                         // инициализируем преиемопередатчик
  attachInterrupt(digitalPinToInterrupt(RF24IRQ), gpio_Handler, FALLING);  
// oldmillis = millis();
}

//-------------------------------------------------------
// главный цикл программы
//-------------------------------------------------------
void loop() {
  //gdb_do_break();
  HTTP.handleClient();                                                // Обработчик HTTP-событий (отлавливает HTTP-запросы к устройству и обрабатывает их в соответствии с выше описанным алгоритмом)
  ftpSrv.handleFTP();                                                 // Обработчик FTP-соединений  

}

