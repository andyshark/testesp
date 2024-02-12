#ifndef __SETWM
#define __SETWM

#include "global_var.h"                 // подключение глобальных переменных

extern void SearchWM(uint64_t Rx1, uint64_t Tx, uint8_t lCh, uint8_t ladr);
void SetWM(uint64_t Rx1, uint64_t Tx, uint8_t lCh, uint8_t ladr, uint64_t sRx1, uint64_t sTx1,
           uint64_t sRx2, uint64_t sTx2, uint8_t sCh, uint8_t sadr, uint8_t ma, uint32_t slp, 
           int cor, uint8_t a1, uint8_t a2, uint8_t cfg, uint8_t aslv1, uint8_t nslv,
           uint32_t toutslv, uint32_t topen);
extern void StoreWM(uint64_t Rx1, uint64_t Tx, uint8_t lCh, uint8_t ladr);
#endif