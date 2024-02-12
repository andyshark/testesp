#include "global_var.h"                                                 // подключение экспорта глобальных переменных  
#include "HttpInit.h"
#include "RF24Net.h"                                                    // подключение определений модуля протокола связи
#include <time.h>      
#include <sys/time.h>             
#include "SetupWS.h"
#include "SetupWM.h"

String message;
String XML;                                                             // для XML запроса

//-------------------------------------------------------
// подпрограммы
//-------------------------------------------------------
void copyCharToStr(char *dst, const String& src) {
  src.toCharArray(dst, sizeof(dst));
}
// создаем xml данные и отправляем
void handleXML() {
  char MyDataTime[80]; // Увеличенный размер буфера символов для хранения отформатированной строки
  time_t now = time(nullptr); // получаем время с помощью библиотеки time.h
  tm* myTime = localtime(&now);
  
  // Форматируем строку с датой и временем
  snprintf(MyDataTime, sizeof(MyDataTime), "%.2d.%.2d.%.4d %.2d:%.2d:%.2d",
           myTime->tm_mday, myTime->tm_mon + 1, myTime->tm_year + 1900,
           myTime->tm_hour, myTime->tm_min, myTime->tm_sec);
  
  String XML = "<?xml version='1.0'?>";
  XML += "<Donnees>";
  XML += "<time>";
  XML += MyDataTime;
  XML += "</time>";
  XML += "</Donnees>";
  
  HTTP.send(200, "text/xml", XML);
}

// Настройка соединения с NTP сервером
void timeSynch(int zone){
  if (WiFi.status() == WL_CONNECTED) {    
    configTime(zone * 3600, 0, "pool.ntp.org", "ru.pool.ntp.org");
    int i = 0;
    while (!time(nullptr) && i < 10) {
      i++;
      delay(1000);
    }    
  }
}

// синхронизация времени
void handle_Time(){
  timeSynch(timezone);
  HTTP.send(200, "text/plain", "OK");                                   // отправляем ответ о выполнении
}

// Установка параметров времянной зоны по запросу вида http://192.168.4.1/TimeZone?timezone=3
void handle_time_zone() {               
  timezone = HTTP.arg("timezone").toInt();                              // Получаем значение timezone из запроса конвертируем в int сохраняем в глобальной переменной
  HTTP.send(200, "text/plain", "OK");
}

//установка времени setTime days.months.years hours:minutes:seconds
void handle_setTime() {
  String MyDataTime =  HTTP.arg("time");
  String tempstr;
  int sc=0, mn=0, hr=0, dy=0, mt=0, yr=0, ms=0;
  unsigned int pos;
  struct tm t = {0};        // Initalize to all 0's
  struct timeval tv;
  pos = MyDataTime.lastIndexOf(':');
  if(pos >= 1) {
    tempstr = MyDataTime.substring(pos +1);
    sc = tempstr.toInt();
    MyDataTime.remove(pos, MyDataTime.length());
  }
  pos = MyDataTime.lastIndexOf(':');
  if(pos >= 1) {
    tempstr = MyDataTime.substring(pos +1);
    mn = tempstr.toInt();
    MyDataTime.remove(pos, MyDataTime.length());
  }
  pos = MyDataTime.lastIndexOf(' ');
  if(pos >= 1) {
    tempstr = MyDataTime.substring(pos +1);
    hr = tempstr.toInt();
    MyDataTime.remove(pos, MyDataTime.length());
  }
  pos = MyDataTime.lastIndexOf('.');
  if(pos >= 1) {
    tempstr = MyDataTime.substring(pos +1);
    yr = tempstr.toInt();
    MyDataTime.remove(pos, MyDataTime.length());
  }
  pos = MyDataTime.lastIndexOf('.');
  if(pos >= 1) {
    tempstr = MyDataTime.substring(pos +1);
    mt = tempstr.toInt();
    MyDataTime.remove(pos, MyDataTime.length());
  }
  dy = MyDataTime.toInt();
  t.tm_year = yr - 1900;    // This is year-1900, so 121 = 2021
  t.tm_mon = mt -1; // -1 ??
  t.tm_mday = dy;
  t.tm_hour = hr;
  t.tm_min = mn;
  t.tm_sec = sc;
  time_t timeSinceEpoch = mktime(&t);
  if (timeSinceEpoch > 2082758399){
	  tv.tv_sec = timeSinceEpoch - 2082758399;  // epoch time (seconds)
  } else {
	  tv.tv_sec = timeSinceEpoch;  // epoch time (seconds)
  }
  tv.tv_usec = ms;    // microseconds
  settimeofday(&tv, NULL);
 HTTP.send(200, "text/plain", "OK"); 
}

// Функция работы с файловой системой
bool handleFileRead(String path){                                     
  if(path.endsWith("/")) path += "index.html";                          // Если устройство вызывается по корневому адресу, то должен вызываться файл index.html (добавляем его в конец адреса)
  String contentType = getContentType(path);                            // С помощью функции getContentType (описана ниже) определяем по типу файла (в адресе обращения) какой заголовок необходимо возвращать по его вызову
  if(LittleFS.exists(path)){                                            // Если в файловой системе существует файл по адресу обращения
    File file = LittleFS.open(path, "r");                               //  Открываем файл для чтения
    HTTP.streamFile(file, contentType);                                 //  Выводим содержимое файла по HTTP, указывая заголовок типа содержимого contentType
    file.close();                                                       //  Закрываем файл
    return true;                                                        //  Завершаем выполнение функции, возвращая результатом ее исполнения true (истина)
  }
  return false;                                                         // Завершаем выполнение функции, возвращая результатом ее исполнения false (если не обработалось предыдущее условие)
}

// Функция, возвращающая необходимый заголовок типа содержимого в зависимости от расширения файла
String getContentType(String filename){                                 
  if (filename.endsWith(".html")) return "text/html";                   // Если файл заканчивается на ".html", то возвращаем заголовок "text/html" и завершаем выполнение функции
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";                // Если файл заканчивается на ".css", то возвращаем заголовок "text/css" и завершаем выполнение функции
  else if (filename.endsWith(".js")) return "application/javascript";   // Если файл заканчивается на ".js", то возвращаем заголовок "application/javascript" и завершаем выполнение функции
  else if (filename.endsWith(".png")) return "image/png";               // Если файл заканчивается на ".png", то возвращаем заголовок "image/png" и завершаем выполнение функции
  else if (filename.endsWith(".jpg")) return "image/jpeg";              // Если файл заканчивается на ".jpg", то возвращаем заголовок "image/jpg" и завершаем выполнение функции
  else if (filename.endsWith(".gif")) return "image/gif";               // Если файл заканчивается на ".gif", то возвращаем заголовок "image/gif" и завершаем выполнение функции
  else if (filename.endsWith(".ico")) return "image/x-icon";            // Если файл заканчивается на ".ico", то возвращаем заголовок "image/x-icon" и завершаем выполнение функции
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";                                                  // Если ни один из типов файла не совпал, то считаем что содержимое файла текстовое, отдаем соответствующий заголовок и завершаем выполнение функции
}

void Msg404() {
  message = "Page Not Found!\n";
  message += "Домовенок не понял команду!\n\n";
  message += "URI: ";
  message += HTTP.uri();
  message += "\nMethod: ";
  message += (HTTP.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += HTTP.args();
  message += "\n";
  for (uint8_t i = 0; i < HTTP.args(); i++) {
    message += " " + HTTP.argName(i) + ": " + HTTP.arg(i) + "\n";
  }
}
// Установка параметров для подключения к внешней AP по запросу вида http://192.168.4.1/ssid?ssid=home2&password=12345678
void handle_Set_Ssid() {
  String sValue = HTTP.arg("ssid");                                     // Получаем значение ssid из запроса
  copyCharToStr(ssid, sValue);                                          // сохраняем в глобальной переменной
  sValue = HTTP.arg("password");                                        // Получаем значение password из запроса
  copyCharToStr(ssid_password, sValue);                                 // сохраняем в глобальной переменной
  HTTP.send(200, "text/plain", "OK");                                   // отправляем ответ о выполнении
}

// Получение параметров для подключения к внешней AP по запросу вида http://192.168.4.1/Gssid
void handle_Get_Ssid() {
  HTTP.send(200, "text/plain", String(ssid) + ":" + ssid_password);     // отправляем ответ о выполнении
}

//Установка параметров внутренней точки доступа по запросу вида http://192.168.4.1/ssidap?ssidAP=home1&passwordAP=8765439
void handle_Set_Ssidap() {                                              //                                       
  String sValue = HTTP.arg("ssidAP");                                   // Получаем значение ssidAP из запроса
  copyCharToStr(ssidAP, sValue);                                        // сохраняем в глобальной переменной
  sValue = HTTP.arg("passwordAP");                                      // Получаем значение passwordAP из запроса
  copyCharToStr(ssidAP_password, sValue);                               // сохраняем в глобальной переменной
  HTTP.send(200, "text/plain", "OK");                                   // отправляем ответ о выполнении
}

// Получение параметров для подключения к внешней AP по запросу вида http://192.168.4.1/Gssid
void handle_Get_SsidAP() {
  HTTP.send(200, "text/plain", String(ssidAP) + ":" + ssidAP_password); // отправляем ответ о выполнении
}

// Сохранение параметров по запросу вида http://192.168.4.1/store_all?device=ok
void handle_Store_All() {                                               //
  String store_all = HTTP.arg("device");                                // Получаем значение device из запроса
  if (store_all == "ok") {                                              // Если значение равно Ок
    HTTP.send(200, "text/plain", "Store OK");                           // Oтправляем ответ Store OK
    Store();                                                            // Функция сохранения данных во Flash
  } else {                                                              // иначе 
    HTTP.send(200, "text/plain", "No Store");                           // Oтправляем ответ No Store
  }
}

// Перезагрузка модуля по запросу вида http://192.168.4.1/restart?device=ok
void handle_Restart() {
  String restart = HTTP.arg("device");                                  // Получаем значение device из запроса
  if (restart == "ok") {                                                // Если значение равно Ок
    HTTP.send(200, "text/plain", "Reset OK");                           // Oтправляем ответ Reset OK
    ESP.restart();                                                      // перезагружаем модуль
  } else {                                                              // иначе
    HTTP.send(200, "text/plain", "No Reset");                           // Oтправляем ответ No Reset
  }
}

// Функция переключения реле 
void relay_switch() {                                              
  byte state;
  if (digitalRead(relay))                                               // Если на пине реле высокий уровень   
    state = 0;                                                          // то запоминаем, что его надо поменять на низкий
  else                                                                  // иначе
    state = 1;                                                          // запоминаем, что надо поменять на высокий
  digitalWrite(relay, state);                                           // меняем значение на пине подключения реле
  HTTP.send(200, "text/plain", String(state)); 							// возвращаем результат, преобразовав число в строку
}

// Функция для определения текущего статуса реле
void relay_status() {                                                  
  byte state;
  if (digitalRead(relay))                                               // Если на пине реле высокий уровень   
    state = 1;                                                          // то запоминаем его как единицу
  else                                                                  // иначе
    state = 0;                                                          // запоминаем его как ноль
  HTTP.send(200, "text/plain", String(state)); 							// возвращаем результат, преобразовав число в строку
}

// поиск модуля протечки http://192.168.4.1/getWM?rx=123456789&tx=012345875&addr=255&ch=100
void handle_search_wsmodule() {
  String MyWM = "";
  SearchWM(HTTP.arg("rx").toInt(), HTTP.arg("tx").toInt(), HTTP.arg("ch").toInt(), HTTP.arg("addr").toInt());
  MyWM += "WM";
  MyWM += ";";
  MyWM += setupWM.RxTube1;
  MyWM += ";";
  MyWM += setupWM.TxTube1;
  MyWM += ";";
  MyWM += setupWM.Address;
  MyWM += ";";
  MyWM += setupWM.Channel;
  MyWM += ";"; 
  MyWM += setupWM.Maddress;
  MyWM += ";";
  MyWM += setupWM.slpDelay;
  MyWM += ";";
  MyWM += setupWM.CorrTermo;
  MyWM += ";";
  MyWM += setupWM.CmpADC_1;
  MyWM += ";";
  MyWM += setupWM.CmpADC_2;
  MyWM += ";";
  MyWM += setupWM.Config;
  MyWM += ";";
  MyWM += setupWM.RxTube2;
  MyWM += ";";
  MyWM += setupWM.TxTube2;
  MyWM += ";";
  MyWM += setupWM.StrtAdrSlv;
  MyWM += ";";
  MyWM += setupWM.NSlv;
  MyWM += ";";
  MyWM += setupWM.SlTimeout;
  MyWM += ";";
  MyWM += setupWM.TOpenClose;
  MyWM += ";";
  HTTP.send(200, "text/plain", MyWM); 
}
// установка параметров модуля протечки http://192.168.4.1/setWM?rx=123456789&tx=012345875&addr=255&ch=100
// &srx1=123456789&stx1=012345875&srx2=123456789&stx2=012345875&saddr=255&ch=100&ma=4&slp=60000&cor=22
// &adc1=90&adc2=90&cfg=3&aslv1=16&nslv=3&toutslv=1000&topen=10000
void handle_set_wsmodule() {
  SetWM(HTTP.arg("rx").toInt(), HTTP.arg("tx").toInt(), HTTP.arg("ch").toInt(), HTTP.arg("addr").toInt(), 
        HTTP.arg("srx1").toInt(), HTTP.arg("stx1").toInt(), HTTP.arg("srx2").toInt(), HTTP.arg("stx2").toInt(),
        HTTP.arg("sch").toInt(), HTTP.arg("saddr").toInt(), HTTP.arg("ma").toInt(),  HTTP.arg("slp").toInt(),  HTTP.arg("cor").toInt(),
        HTTP.arg("adc1").toInt(), HTTP.arg("adc2").toInt(), HTTP.arg("cfg").toInt(), HTTP.arg("aslv1").toInt(),
         HTTP.arg("nslv").toInt(), HTTP.arg("toutslv").toInt(), HTTP.arg("topen").toInt());
   HTTP.send(200, "text/plain", "");                         
}
// поиск датчика протечки http://192.168.4.1/storeWM?rx=123456789&tx=012345875&addr=255&ch=100
void handle_store_wsmodule() { 
 StoreWM(HTTP.arg("rx").toInt(), HTTP.arg("tx").toInt(), HTTP.arg("ch").toInt(), HTTP.arg("addr").toInt());
 HTTP.send(200, "text/plain", ""); 
}

// поиск датчика протечки http://192.168.4.1/getWS?rx=123456789&tx=012345875&addr=255&ch=100
void handle_search_wssensor() {
  String MyWS = "";
  SearchWS(HTTP.arg("rx").toInt(), HTTP.arg("tx").toInt(), HTTP.arg("ch").toInt(), HTTP.arg("addr").toInt());
  MyWS += "WS";
  MyWS += ";";
  MyWS += setupWM.RxTube1;
  MyWS += ";";
  MyWS += setupWM.TxTube1;
  MyWS += ";";
  MyWS += setupWM.Address;
  MyWS += ";";
  MyWS += setupWM.Channel;
  MyWS += ";"; 
  MyWS += setupWM.Maddress;
  MyWS += ";";
  MyWS += setupWM.slpDelay;
  MyWS += ";";
  MyWS += setupWM.CorrTermo;
  MyWS += ";";
  MyWS += setupWM.CmpADC_1;
  MyWS += ";";
  MyWS += setupWM.CmpADC_2;
  MyWS += ";";
  MyWS += setupWM.Config;
  HTTP.send(200, "text/plain", MyWS); 
}
 
// установка параметров датчика протечки http://192.168.4.1/setWS?rx=123456789&tx=012345875&addr=255&ch=100
// &srx=123456789&stx=012345875&saddr=255&sch=100&ma=4&slp=60000&cor=22&adc1=90&adc2=90&cfg=3   
void handle_set_wssensor() {
  SetWS(HTTP.arg("rx").toInt(), HTTP.arg("tx").toInt(), HTTP.arg("ch").toInt(), HTTP.arg("addr").toInt(), 
        HTTP.arg("srx").toInt(), HTTP.arg("stx").toInt(), HTTP.arg("sch").toInt(), HTTP.arg("saddr").toInt(),
        HTTP.arg("ma").toInt(),  HTTP.arg("slp").toInt(),  HTTP.arg("cor").toInt(),
        HTTP.arg("adc1").toInt(), HTTP.arg("adc2").toInt(), HTTP.arg("cfg").toInt());
   HTTP.send(200, "text/plain", "");                         
}

// поиск датчика протечки http://192.168.4.1/storeWS?rx=123456789&tx=012345875&addr=255&ch=100
void handle_store_wssensor() { 
 StoreWS(HTTP.arg("rx").toInt(), HTTP.arg("tx").toInt(), HTTP.arg("ch").toInt(), HTTP.arg("addr").toInt());
 HTTP.send(200, "text/plain", ""); 
}

void LfsFtpHttpInit() {
  LittleFS.begin();                                                     // Инициализируем работу с файловой системой                          
  ftpSrv.begin(www_username, www_password);                             // Поднимаем FTP-сервер для удобства отладки работы HTML 
  HTTP.begin();                                                         // Инициализируем Web-сервер

  // Обработка HTTP-запросов
  // WIFI
  HTTP.on("/ssid", handle_Set_Ssid);                                    // Установить имя и пароль роутера по запросу вида http://192.168.4.1/ssid?ssid=home2&password=12345678
  HTTP.on("/Gssid", handle_Get_Ssid);                                   // Получить имя и пароль роутера по запросу вида http://192.168.4.1/Gssid
  HTTP.on("/GssidAP", handle_Get_SsidAP);                               // Получить имя и пароль роутера по запросу вида http://192.168.4.1/GssidAP
  HTTP.on("/ssidap", handle_Set_Ssidap);                                // Установить имя и пароль для точки доступа по запросу вида http://192.168.4.1/ssidap?ssidAP=home1&passwordAP=8765439
  HTTP.on("/store_all", handle_Store_All);                              // Сохранение параметров по запросу вида http://192.168.4.1/store_all?device=ok
  HTTP.on("/restart", handle_Restart);                                  // Перезагрузка модуля по запросу вида http://192.168.4.1/restart?device=ok
  HTTP.on("/Time", handle_Time);                                        // Синхронизировать время устройства по запросу вида http://192.168.4.1/Time
  HTTP.on("/setTime", handle_setTime);                                  // Синхронизировать время устройства по запросу вида http://192.168.4.1/Time?1 1 1970 0:0:0
  HTTP.on("/TimeZone", handle_time_zone);                               // Установка времянной зоны по запросу вида http://192.168.4.1/TimeZone?timezone=5
  timeSynch(timezone);
  HTTP.on("/xml",handleXML);                                            // формирование xml страницы для передачи данных в web интерфейс
  // water sensor
  HTTP.on("/getWS", handle_search_wssensor);                            // поиск датчика протечки http://192.168.4.1/getWS?rx=123456789&tx=012345875&addr=255&ch=100
  HTTP.on("/setWS", handle_set_wssensor);                               // установка параметров датчика протечки http://192.168.4.1/setWS?rx=123456789&tx=012345875&addr=255&ch=100
                                                                        // srx=123456789&stx=012345875&saddr=255&ch=100&ma=4&slp=60000&cor=22&adc1=90&adc2=90&cfg=3
  HTTP.on("/storeWS", handle_store_wssensor);                           // поиск датчика протечки http://192.168.4.1/storeWS?rx=123456789&tx=012345875&addr=255&ch=100
  // water module
  HTTP.on("/getWM", handle_search_wsmodule);                            // поиск датчика протечки http://192.168.4.1/getWM?rx=123456789&tx=012345875&addr=255&ch=100
  HTTP.on("/setWM", handle_set_wsmodule);                               // установка параметров модуля протечки http://192.168.4.1/setWM?rx=123456789&tx=012345875&addr=255&ch=100
                                                                        // &srx1=123456789&stx1=012345875&srx2=123456789&stx2=012345875&saddr=255&ch=100&ma=4&slp=60000&cor=22&adc1=90&adc2=90&cfg=3
                                                                        // &aslv1=16&nslv=3&toutslv=1000&topen=10000
  HTTP.on("/storeWM", handle_store_wsmodule);                           // поиск датчика протечки http://192.168.4.1/storeWM?rx=123456789&tx=012345875&addr=255&ch=100

  HTTP.on("/relay_switch", relay_switch);                               // При HTTP запросе вида http://192.168.4.1/relay_switch
  HTTP.on("/relay_status", relay_status);                               // При HTTP запросе вида http://192.168.4.1/relay_status

  HTTP.onNotFound([](){                                                 // Описываем действия при событии "Не найдено"
    if (!HTTP.authenticate(strcpy(new char[sizeof(www_username)], www_username), 
        strcpy(new char[sizeof(www_password)], www_password))) {
     return HTTP.requestAuthentication(DIGEST_AUTH, "Введите логин и пароль", "Неверный логин или пароль");   //...вернуться и повторить попытку ввода.
    }
    if(!handleFileRead(HTTP.uri())) {                                   // Если функция handleFileRead (описана ниже) возвращает значение false в ответ на поиск файла в файловой системе
     Msg404(); 
     HTTP.send(404, "text/plain", message);                             // возвращаем на запрос текстовое сообщение "File isn't found" с кодом 404 (не найдено)
    }
  });    
}