#include "global_var.h"
//-------------------------------------------------------
// прототипы функций
//-------------------------------------------------------
extern void relay_switch();
extern void relay_status();
extern String getContentType(String filename);
extern bool handleFileRead(String path);
extern void LfsFtpHttpInit();
// работа с часами
extern void Time_init();
extern void timeSynch(int zone);
extern void handle_time_zone();
extern void handle_Time();
String formatNumber(int num, int digits);

