#ifndef __SETWS
#define __SETWS

#include "global_var.h"                 // подключение глобальных переменных

extern void SearchWS(uint64_t Rx1, uint64_t Tx, uint8_t lCh, uint8_t ladr);
extern void SetWS(uint64_t Rx1, uint64_t Tx, uint8_t lCh, uint8_t ladr, 
                  uint64_t sRx1, uint64_t sTx, uint8_t sCh, uint8_t sadr, uint8_t ma,
                  uint32_t slp, int cor, uint8_t a1, uint8_t a2, uint8_t cfg);
extern void StoreWS(uint64_t Rx1, uint64_t Tx, uint8_t lCh, uint8_t ladr);
#endif