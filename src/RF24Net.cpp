#include "RF24Net.h"                    // подключение определений модуля протокола связи
#include "RF24.h"                       // подключение библиотеки NRF24L01
#include "global_var.h"                 // подключение глобальных переменных
#include "EpromVar.h"                   // импортируем библиотеку

//#include <SPI.h>
#include "printf.h"
//#include "RF24.h"
//-------------------------------------------------------
// переменные 
//-------------------------------------------------------
 RF24 radio(RF24CEpin, RF24CSNpin);     // Настройка nRF24L01 на SPI и пины 7(CE) & 8(CSN)

//-------------------------------------------------------
// настройка модуля
//-------------------------------------------------------
void SetupRadio(void) {  
 printf_begin();
 radio.begin();                         // инициализация SPI, ce(LOW), csn(HIGH)
 radio.setAutoAck(true);                // режим подтверждения приёма
 radio.setRetries(15, 2);               // (время между попыткой достучаться, число попыток)
 radio.disableAckPayload();             // запретим добавлять полезную нагрузку в автоподтверждении
 radio.setDataRate(RF24_1MBPS);         // скорость обмена данными RF24_250KBPS
 radio.setCRCLength(RF24_CRC_8);        // размер контрольной суммы 8 bit или 16 bit
 radio.setChannel(Channel);             // установка канала
 radio.setPALevel(RF24_PA_MAX);         // RF24_PA_MAX по умолчанию.
 radio.setPayloadSize(13);              // размер пакета, в байтах
 radio.openWritingPipe(TxTube);         // труба 0, открываем канал для передачи данных
 radio.openReadingPipe(1, RxTube1);     // хотим слушать трубу 1
 radio.openReadingPipe(2, RxTube2);     // хотим слушать трубу 2
 radio.maskIRQ(true, false, false);     // args = "data_sent", "data_fail", "data_ready"
 radio.startListening();                // переключение на прием
 radio.powerUp();                       // включение или пониженное потребление powerDown - powerUp 
 /*
 radio.printDetails();
 Serial.println(F("----------------"));
 radio.printPrettyDetails(); 
 Serial.println(F("----------------"));
 */
}

//-------------------------------------------------------
// шифровка пакета
//-------------------------------------------------------
IRAM_ATTR void CryptPkt(void* buf, uint8_t len) {
  uint8_t* current = reinterpret_cast<uint8_t*>(buf); // преобразуем указатель в указатель другого типа
  current[len - 1] = current[0];
  for (uint8_t i = 1; i < len - 1; i++) 
  {
   current[len - 1] += current[i];
   current[i] ^= current[i-1];  
  }
}

//-------------------------------------------------------
// дешифровка пакета
//-------------------------------------------------------
IRAM_ATTR void DecryptPkt(void* buf, uint8_t len) {
  uint8_t* current = reinterpret_cast<uint8_t*>(buf); // преобразуем указатель в указатель другого типа
  uint8_t newkey, oldkey;

  current[len - 1] -= current[0];
  oldkey=current[0];
  for (uint8_t i = 1; i < len - 1; i++) 
  {
   newkey = current[i];   
   current[i] ^= oldkey;   
   oldkey = newkey;
   current[len - 1] -= current[i];
  }
}

//-------------------------------------------------------
// обработка принятого пакета
//-------------------------------------------------------
IRAM_ATTR void radioSetCMD(void) {       
  bool transmit;                                      // флаг - команда содержит ответ        
  uint8_t pipe_num = 0;                               // номер канала приславшего сообщение
  if (radio.available(&pipe_num)) {                   // проверяем пришло ли что-то
    radio.read(&SLpkt, sizeof(SLpkt));                // забираем данные
    if (SLpkt.DstAdr != Address) {                    // пакет для этого модуля?
        memset(SLpkt.buf, 0, sizeof(SLpkt.buf));      // нет, очистим пакет
        return;                                       // выходим
    }

    DecryptPkt(&SLpkt, sizeof(SLpkt));                 // да, расшифруем пакет    
    transmit = false;                                  // сбросим флаг - команда содержит ответ
    if(SLpkt.crc != 0) {                               // проверим кс =0
      SLpkt.CMD = 255;                                 // нет, фальшивый пакет
    }

    switch(SLpkt.CMD)
    {  
     case 53:  // текущие значения главного модуля протечки

            break;      

     case 54:  // текущие значения датчиков протечки
            uint8_t NumSl;                     // номер слейва приславшого пакет
            NumSl = SLpkt.SrcAdr - StrtAdrSlv; // определим номер слейва 0..15
            if ((SLpkt.SrcAdr >= StrtAdrSlv) &&
                (SLpkt.SrcAdr <= StrtAdrSlv + 16)) {
             if ((NSlv > 0) && (NSlv >= NumSl)) {           // адрес слейва в диапазоне
                memcpy(FarSLpkt[NumSl][0].buf, SLpkt.buf, sizeof(FarSLpkt[0][0].buf)); // копируем пакет в память
                FarSLpkt[NumSl][0].oldTime = millis();                              // запомним время прихода пакета
                SLpkt.DstAdr = SLpkt.SrcAdr;
                SLpkt.SrcAdr = Address;                                          // кто
                SLpkt.CMD = 100;                                                 // команда квитирования слева 101, должна быть отправлена менее чем через 10мс
                transmit = true;                                                 // ответ разрешен
             }
            }
            break;      
                                                                               
     case 198: // чтение установленных параметров RxTube
            setupWM.RxTube2 = (uint64_t&)SLpkt.buf[0]; 
            break; 

     case 199: // чтение установленных параметров TxTube                             
            setupWM.TxTube2 = (uint64_t&)SLpkt.buf[0];                  
            break;  
 
     case 200: // чтение установленных параметров TxTube                             
            setupWM.Maddress = (uint8_t&)SLpkt.buf[0];
            setupWM.NSlv = (uint8_t&)SLpkt.buf[1];  
            setupWM.SlTimeout = (uint32_t&)SLpkt.buf[2];  
            setupWM.StrtAdrSlv = (uint8_t&)SLpkt.buf[3];                               
            break;

     case 201: // чтение аварийное время переключения задвижки ms
            setupWM.TOpenClose = (uint32_t&)SLpkt.buf[0];
            break;

     case 202: // чтение задержки сна ms
            setupWM.slpDelay = (uint32_t&)SLpkt.buf[0];
            break;
                                                   
     case 203: // чтение установленных параметров RxTube
            setupWM.RxTube1 = (uint64_t&)SLpkt.buf[0]; 
            break;

     case 204: // чтение установленных параметров TxTube                             
            setupWM.TxTube1 = (uint64_t&)SLpkt.buf[0];                 
            break; 
            
     case 205: // чтение установленных параметров адрес прибора, конфиг датчиков, коррекция термодатчика кристалла
            setupWM.Address = (uint8_t&)SLpkt.buf[0]; 
            setupWM.Channel = (uint8_t&)SLpkt.buf[1];
            setupWM.CorrTermo = (int16_t&)SLpkt.buf[2];  
            setupWM.Config = (uint8_t&)SLpkt.buf[4];
            setupWM.CmpADC_1 = (uint8_t&)SLpkt.buf[5];
            setupWM.CmpADC_2 = (uint8_t&)SLpkt.buf[6];  
            setupWM.Maddress = (uint8_t&)SLpkt.buf[7];            // адрес мастера         
            break;

     case 254: // перезагрузка
            Store();
            ESP.restart();
            break;
                        
     case 255: // не верная кс    
     default:    
            memset(SLpkt.buf, 0, sizeof(SLpkt.buf));
            break;
    }
    ///////////////////////////////////////////
    // если команда была с ответом, ответим
    ///////////////////////////////////////////
    if(transmit) {
      SendPacket(pipe_num);                         // передача пакета
      transmit = false;                             // ответ запрещен
    }
  }  
}  

//-------------------------------------------------------
// отправить пакет
//-------------------------------------------------------
IRAM_ATTR void SendPacket(uint8_t nTube) {
 CryptPkt(&SLpkt, sizeof(SLpkt));               //
 radio.stopListening();                         // ce(LOW), радиоэфир не слушаем, только передача
 if(nTube == 1) {
   radio.openWritingPipe(TxTube);               //
 } 
 if(nTube == 2) {
   radio.openWritingPipe(TxTube2);              //
 }
 radio.write(&SLpkt, sizeof(SLpkt));            // ответ
 memset(SLpkt.buf, 0, sizeof(SLpkt.buf));       //
 radio.startListening();                        // переключение на прием
}

//-------------------------------------------------------
// запись текущих значений для отправки по запросу
//-------------------------------------------------------
IRAM_ATTR void SendSensorParam(uint8_t nTube, uint8_t AddrWho) { 
 noInterrupts();
 SLpkt.SrcAdr = Address;                             // кто      
 SLpkt.DstAdr = AddrWho;                             // кому (Maddress)       
 SLpkt.CMD = 101;                                    // Зададим CMD, т.к при вызове по прерыванию CMD не задан
 (uint8_t&)SLpkt.buf[0] = Vbat;                      // напряжение питания
 (unsigned int&)SLpkt.buf[1] = DTermo;               // температура кристалла
 (uint8_t&)SLpkt.buf[3] = Alarm;                     // локальная зона протечки            
 (uint8_t&)SLpkt.buf[4] = StatCool;                  // состяние задвижки холодной
 (uint8_t&)SLpkt.buf[5] = StatHot;                   // состяние задвижки горячей
 (uint8_t&)SLpkt.buf[6] = ExternAlm1;                // ошибка на внешнем модуле
 (uint8_t&)SLpkt.buf[7] = ExternAlm2;                // ошибка на внешнем модуле
 SendPacket(nTube);                                  // передаем и включаем приемник
 interrupts();
}

//-------------------------------------------------------
// запись уставок
//-------------------------------------------------------
void Store(void){
 EEPROM_uint64_write(eTxTube, TxTube);
 EEPROM_uint64_write(eRxTube1, RxTube1);
 EEPROM.write(eChannel, Channel);
 EEPROM.write(eAddress, Address);
 EEPROM_int_write(eCorrTermo, CorrTermo);
 EEPROM.write(eConfig, Config);
 EEPROM_ulong_write(eslpDelay, slpDelay);
 EEPROM.write(eCmpADC_1, CmpADC_1);
 EEPROM.write(eCmpADC_2, CmpADC_2);
 EEPROM.write(eMaddress, Maddress);
 EEPROM.write(eStrtAdrSlv, StrtAdrSlv);
 EEPROM.write(eNSlv, NSlv);
 //EEPROM_ulong_write(eTOpenClose, TOpenClose);
 EEPROM_uint64_write(eRxTube2, RxTube2);
 EEPROM_uint64_write(eTxTube2, TxTube2); 
 EEPROM_ulong_write(eSlTimeout, SlTimeout); 
 EEPROM_String_write(ewww_username, www_username);
 EEPROM_String_write(ewww_password, www_password);
 EEPROM_String_write(essid, ssid);
 EEPROM_String_write(essid_password, ssid_password); 
 EEPROM_String_write(essidAP, ssidAP);
 EEPROM_String_write(essidAP_password, ssidAP_password);  // не более 20
 EEPROM_int_write(etimezone, timezone);
 EECRC = eeprom_onA4_crc();              // контрольная сумма епром
 EEPROM_ulong_write(eEECRC, EECRC); 
 Serial.println();
}

//-------------------------------------------------------
// причина прерывания
//-------------------------------------------------------
void whyIrq(void) {
  bool tx_ds, tx_df, rx_dr;                 // маски IRQ: Данные отправлены, Ошибка данных, Данные готовы 
  radio.whatHappened (tx_ds, tx_df, rx_dr); // получаем значения масок IRQ и сбросим их
  if(rx_dr) {                               // по приему
    radioSetCMD();                          // обработаем запрос
  }
}

