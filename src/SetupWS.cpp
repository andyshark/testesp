#include "SetupWS.h"
#include "RF24Net.h"

//-------------------------------------------------------
// поиск модуля для поиска датчика протечки
//-------------------------------------------------------
void SearchWS(uint64_t Rx1, uint64_t Tx, uint8_t lCh, uint8_t ladr) {  
 uint64_t oldTxTube, oldRxTube1;
 uint8_t oldChannel, j;

 setupWM.Address = 0;                   // адрес прибора
 setupWM.Channel = 0;                   // канал
 setupWM.CorrTermo = 0;                 // значение поправки датчика температуры кристалла
 setupWM.Config = 0;                    // конфигурация
 setupWM.CmpADC_1 = 0;                  // значение порога для ацп 1
 setupWM.CmpADC_2 = 0;                  // значение порога для ацп 2
 setupWM.Maddress = 0;                  // адрес мастера       
 setupWM.TxTube1 = 0;                   // труба
 setupWM.RxTube1 = 0;                   // труба
 setupWM.TxTube2 = 0;                   // труба
 setupWM.RxTube2 = 0;                   // труба 
 setupWM.slpDelay = 0;                  // время спячки ms
 setupWM.StrtAdrSlv = 0;                // начальный адрес с которого в порядке возрастания идут до 16 адресов подчиненных
 setupWM.NSlv = 0;                      // количество подчиненных модулей
 setupWM.SlTimeout = 0;                 // общий таймаут для любого слейва 
 setupWM.TOpenClose = 0;                // аварийное время переключения задвижки ms

 oldTxTube = TxTube; 
 oldRxTube1 = RxTube1;
 oldChannel = Channel;
 TxTube = Tx; 
 RxTube1 = Rx1;
 Channel = lCh;
 SetupRadio();
 delay(100);
 
 for(j = 50; j < 54; j++) {              // чтение Address ...,TxTube, RxTube, slpDelay
  noInterrupts();
  SLpkt.SrcAdr = Address;                // кто      
  SLpkt.DstAdr = ladr;                   // кому        
  SLpkt.CMD = j;                         // Зададим CMD
  SendPacket(1); 
  interrupts();
  delay(300);
 }

 TxTube = oldTxTube; 
 RxTube1 = oldRxTube1;
 Channel = oldChannel; 
 SetupRadio();
 delay(100);
}

//-------------------------------------------------------
// настройка модуля для поиска датчика протечки
//-------------------------------------------------------
void SetWS(uint64_t Rx1, uint64_t Tx, uint8_t lCh, uint8_t ladr, uint64_t sRx1, uint64_t sTx, 
           uint8_t sCh, uint8_t sadr, uint8_t ma, uint32_t slp, int cor, uint8_t a1, uint8_t a2, uint8_t cfg ) { 
 uint64_t oldTxTube, oldRxTube1;
 uint8_t oldChannel;

 setupWM.Address = 0;
 setupWM.Channel = 0;
 setupWM.CorrTermo = 0;  
 setupWM.Config = 0;
 setupWM.CmpADC_1 = 0;
 setupWM.CmpADC_2 = 0;   
 setupWM.Maddress = 0;                           
 setupWM.TxTube1 = 0;                  
 setupWM.RxTube1 = 0; 
 setupWM.slpDelay = 0;

 oldTxTube = TxTube; 
 oldRxTube1 = RxTube1;
 oldChannel = Channel;
 TxTube = Tx; 
 RxTube1 = Rx1;
 Channel = lCh;
 SetupRadio();
 delay(100);
 noInterrupts();
 SLpkt.SrcAdr = Address;                 // кто      
 SLpkt.DstAdr = ladr;                    // кому        
 SLpkt.CMD = 0;                          // Зададим CMD
 (uint8_t&)SLpkt.buf[0] = sadr;
 (uint8_t&)SLpkt.buf[1] = sCh;
 (int16_t&)SLpkt.buf[2] = cor;  
 (uint8_t&)SLpkt.buf[4] = cfg;
 (uint8_t&)SLpkt.buf[5] = a1;
 (uint8_t&)SLpkt.buf[6] = a2;   
 (uint8_t&)SLpkt.buf[7] = ma;            // адрес мастера 
 SendPacket(1); 
 interrupts();
 delay(100);
 noInterrupts();
 SLpkt.SrcAdr = Address;                 // кто      
 SLpkt.DstAdr = ladr;                    // кому        
 SLpkt.CMD = 1;                          // Зададим CMD
 (uint64_t&)SLpkt.buf[0] = sTx;
 SendPacket(1); 
 interrupts();
 delay(100);
 noInterrupts();
 SLpkt.SrcAdr = Address;                 // кто      
 SLpkt.DstAdr = ladr;                    // кому        
 SLpkt.CMD = 2;                          // Зададим CMD
 (uint64_t&)SLpkt.buf[0] = sRx1; 
 SendPacket(1); 
 interrupts();
 delay(100);
 noInterrupts();
 SLpkt.SrcAdr = Address;                 // кто      
 SLpkt.DstAdr = ladr;                    // кому        
 SLpkt.CMD = 3;                          // Зададим CMD
 (uint32_t&)SLpkt.buf[0] = slp;
 SendPacket(1); 
 interrupts();
 delay(100); 
 TxTube = oldTxTube; 
 RxTube1 = oldRxTube1;
 Channel = oldChannel; 
 SetupRadio();
 delay(100);
}

//-------------------------------------------------------
// запись crc в eeprom датчика протечки
//-------------------------------------------------------
void StoreWS(uint64_t Rx1, uint64_t Tx, uint8_t lCh, uint8_t ladr) {
 uint64_t oldTxTube, oldRxTube1;
 uint8_t oldChannel;

 oldTxTube = TxTube; 
 oldRxTube1 = RxTube1;
 oldChannel = Channel;
 TxTube = Tx;          
 RxTube1 = Rx1;        
 Channel = lCh;         
 SetupRadio();
 delay(100);
 noInterrupts();
 SLpkt.SrcAdr = Address;                 // кто      
 SLpkt.DstAdr = ladr;                    // кому        
 SLpkt.CMD = 254;                        // Зададим CMD
 SendPacket(1);  
 interrupts();
 delay(100);
 TxTube = oldTxTube; 
 RxTube1 = oldRxTube1;
 Channel = oldChannel; 
 SetupRadio();
 delay(100);
}