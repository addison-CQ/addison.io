#include "esp32-hal-cpu.h"
#include <stdio.h>
#include <string.h>
int initFlag = 0;  //硬件初始化标志
int intFlag = 0;   //按键1中断标志位
int count = 0;
char point[11];
// #define USART_DEBUG 1   //使能串口打印调试信息
//RTC
#include <ESP32Time.h>
ESP32Time rtc(0);  // offset in seconds GMT+1
//OLED
#include <U8g2lib.h>
#include <Wire.h>
//SD Card
#include "FS.h"
#include "SD.h"
#include "SPI.h"
//OLED
#define OLED_SCL 25
#define OLED_SDA 26
//WIFI
#include <WiFi.h>
#include <WebServer.h>
//ADC->PulseSensor
#define USE_ARDUINO_INTERRUPTS false
#include <PulseSensorPlayground.h>
uint16_t dataHR = 0;
struct Data {
  char HR[10] = "HR:0";
  char SPO2[10] = "SPO2:N/A";
} adcData;
PulseSensorPlayground pulseSensor;
const int OUTPUT_TYPE = SERIAL_PLOTTER;
const int PULSE_INPUT = 36;
const int THRESHOLD = 850;  // Adjust this number to avoid noise when idle
byte samplesUntilReport;
const byte SAMPLES_PER_SERIAL_SAMPLE = 1000000;
//OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /*reset=*/U8X8_PIN_NONE, /*clock=*/OLED_SCL, /*data=*/OLED_SDA);  //U8G2程序库 SSD1306控制晶片 128X64_NONAME解析度和型号 F暂存区大小可以改为1或2 F_HW_I2C控制介面 u8g2(U8G2_R0代表不旋转,U8X8_PIN_NONE代表没有重置引脚)
//WIFI
const char *ssid = "ESP32 Sensor Kit";  // Enter your SSID here  // SSID & Password
const char *password = "12345678";      //Enter your Password here
WebServer server(80);                   // Object of WebServer(HTTP port, 80 is defult)
String strHR;
String strSPO2;
String HTML;
//Timer
hw_timer_t *timer = NULL;

//按键1中断服务函数
ICACHE_RAM_ATTR void INTfunction() {
  if (intFlag == 0)
    intFlag = 1;
  else
    intFlag = 0;
}

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(240);
#ifdef USART_DEBUG
  Serial.print("CpuFrequency:");
  Serial.print(getCpuFrequencyMhz());
  Serial.println("MHz");
#endif
  /*
   * RTC
   */
  rtc.setTime(0, 0, 14, 7, 11, 2022);  // 7th Nov 2021 14:0:0
  /*
   * OLED
   */
  u8g2.setI2CAddress(0x78);
  u8g2.begin();
  u8g2.enableUTF8Print();  // enable UTF8 support for the Arduino print() function
  u8g2.setFont(u8g2_font_unifont_t_symbols);
  u8g2.clear();
  u8g2.clearBuffer();
  u8g2.firstPage();
  do {
    u8g2.setCursor(0, 14);           //指定显示位置
    u8g2.print("ESP32 Sensor Kit");  //使用print来显示字符串
    u8g2.setCursor(0, 35);           //指定显示位置
    u8g2.print("Booting...");        //使用print来显示字符串
  } while (u8g2.nextPage());
  /*
   * SD卡
   */
  if (!SD.begin()) {  //检测SD卡挂载状态
    u8g2.clearBuffer();
    u8g2.firstPage();
    do {
      u8g2.setCursor(0, 14);           //指定显示位置
      u8g2.print("ESP32 Sensor Kit");  //使用print来显示字符串
      u8g2.setCursor(0, 35);           //指定显示位置
      u8g2.print("Card Mount");        //使用print来显示字符串
      u8g2.setCursor(0, 55);           //指定显示位置
      u8g2.print("Failed !!!");        //使用print来显示字符串
    } while (u8g2.nextPage());
    Serial.println("Card Mount Failed !");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
  listDir(SD, "/", 0);
  /*
   * WIFI
   */
  Serial.println("Try Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);              // Connect to your wi-fi modem
  while (WiFi.status() != WL_CONNECTED) {  // Check wi-fi is connected to wi-fi network
    point[count] = '.';
    u8g2.clearBuffer();
    u8g2.firstPage();
    do {
      u8g2.setCursor(0, 14);           //指定显示位置
      u8g2.print("ESP32 Sensor Kit");  //使用print来显示字符串
      u8g2.setCursor(0, 35);           //指定显示位置
      u8g2.print("Connect to WIFI");   //使用print来显示字符串
      u8g2.setCursor(0, 55);           //指定显示位置
      u8g2.print(point);               //使用print来显示字符串
    } while (u8g2.nextPage());
    if (count == 10) {
      memset(point, 0, sizeof(char) * 10);
      count = 0;
    } else
      count++;
    Serial.print(".");
    delay(500);
  }
  memset(point, 0, sizeof(char) * 10);
  count = 0;
  Serial.println("");
  Serial.println("WiFi connected successfully");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());  //Show ESP32 IP on serial
  server.on("/", handle_root);
  server.begin();
  Serial.println("HTTP server started");
  u8g2.clearBuffer();
  u8g2.firstPage();
  do {
    u8g2.setCursor(0, 14);           //指定显示位置
    u8g2.print("ESP32 Sensor Kit");  //使用print来显示字符串
    u8g2.setCursor(0, 35);           //指定显示位置
    u8g2.print("My IP Address:");    //使用print来显示字符串
    u8g2.setCursor(0, 55);           //指定显示位置
    u8g2.print(WiFi.localIP());      //使用print来显示字符串
  } while (u8g2.nextPage());
  xTaskCreatePinnedToCore(
    wifiTask,
    "wifiTask", /* 任务名称. */
    4096,       /* 任务的堆栈大小 */
    NULL,       /* 任务的参数 */
    3,          /* 任务的优先级 */
    NULL,       /* 跟踪创建的任务的任务句柄 */
    1);         /* pin任务到核心0 */
  delay(5000);
  /*
   * ADC->PulseSensor
   */
  pulseSensor.analogInput(PULSE_INPUT);
  pulseSensor.setSerial(Serial);
  pulseSensor.setOutputType(OUTPUT_TYPE);
  pulseSensor.setThreshold(THRESHOLD);
  samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;
  if (!pulseSensor.begin()) {
    u8g2.clearBuffer();
    u8g2.firstPage();
    do {
      u8g2.setCursor(0, 14);           //指定显示位置
      u8g2.print("ESP32 Sensor Kit");  //使用print来显示字符串
      u8g2.setCursor(0, 35);           //指定显示位置
      u8g2.print("PulseSensor Init");  //使用print来显示字符串
      u8g2.setCursor(0, 55);           //指定显示位置
      u8g2.print("Failed !!!");        //使用print来显示字符串
    } while (u8g2.nextPage());
    Serial.println("PulseSensor Init Failed !");
    return;
  }
  timer = timerBegin(0, 80, true);              //初始化
  timerAttachInterrupt(timer, &onTimer, true);  //调用中断函数
  timerAlarmWrite(timer, 1000, true);           //timerBegin的参数二 80位80MHZ，这里为2000  意思为2毫秒
  timerAlarmEnable(timer);                      //定时器使能
  //使能按键1中断监测
  attachInterrupt(4, INTfunction, FALLING);
  //硬件初始化成功 标志置一
  initFlag = 1;
}

void loop() {
  //初始化失败则退出程序
  if (initFlag == 0) {
    Serial.print("Hardware Init Failed !\r\n");
    delay(3000);
    ESP.restart();
  }
  //如果ADC->PulseSensor采集到了数据
#ifdef USART_DEBUG
  Serial.print("HR: ");
  Serial.println(dataHR);
#endif
  char temp[10];
  memset(adcData.HR, 0, sizeof(char) * 10);
  adcData.HR[0] = 'H';
  adcData.HR[1] = 'R';
  adcData.HR[2] = ':';
  itoa(dataHR, temp, 10);
  for (int i = 0; i < strlen(temp); i++) {
    adcData.HR[3 + i] = temp[i];
  }
  /*
   * OLED
   */
  u8g2.clearBuffer();
  u8g2.firstPage();
  do {
    if (adcData.HR[3] == '0') {
      point[count] = '.';
      u8g2.setCursor(0, 14);           //指定显示位置
      u8g2.print("ESP32 Sensor Kit");  //使用print来显示字符串
      u8g2.setCursor(0, 35);           //指定显示位置
      u8g2.print("Please keep");       //使用print来显示字符串
      if (intFlag == 1) {
        u8g2.setCursor(100, 40);  //指定显示位置
        u8g2.print("REC");        //使用print来显示字符串
      }
      u8g2.setCursor(0, 55);       //指定显示位置
      u8g2.print("touch stable");  //使用print来显示字符串
      u8g2.setCursor(101, 55);     //指定显示位置
      u8g2.print(point);           //使用print来显示字符串
      if (count == 2) {
        memset(point, 0, sizeof(char) * 10);
        count = 0;
      } else
        count++;
    } else {
      count = 0;
      memset(point, 0, sizeof(char) * 10);
      u8g2.setCursor(0, 14);           //指定显示位置
      u8g2.print("ESP32 Sensor Kit");  //使用print来显示字符串
      u8g2.setCursor(0, 35);           //指定显示位置
      u8g2.print(adcData.HR);          //使用print来显示字符串
      if (intFlag == 1) {
        u8g2.setCursor(100, 40);  //指定显示位置
        u8g2.print("REC");        //使用print来显示字符串
      }
      u8g2.setCursor(0, 55);     //指定显示位置
      u8g2.print(adcData.SPO2);  //使用print来显示字符串
    }
  } while (u8g2.nextPage());
  /*
    * SD Card
    */
  if (intFlag == 1) {
    if (adcData.HR[3] != '0') {
      appendFile(SD, "/PulseSensor_WLAN_STA.txt", &(rtc.getTime("%B %d %Y %H:%M:%S"))[0]);
      appendFile(SD, "/PulseSensor_WLAN_STA.txt", "\t\t");
      appendFile(SD, "/PulseSensor_WLAN_STA.txt", adcData.HR);
      appendFile(SD, "/PulseSensor_WLAN_STA.txt", "\r\n");
    }
  }
  adcData.HR[3] == '0';
  dataHR = 0;
  delay(1000);
}

/*
 * ADC->PulseSensor
 */
void onTimer()  //PulseSensor采集数据
{
  if (pulseSensor.sawNewSample()) {
    if (--samplesUntilReport == (byte)0) {
      samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;
      pulseSensor.outputSample();
      if (pulseSensor.sawStartOfBeat()) {
        dataHR = pulseSensor.getBeatsPerMinute();
      }
    }
  }
}

/*
 * WIFI
 */
void wifiTask(void *pvParameters) {  //蓝牙外发数据
  while (1) {
    strHR = String(adcData.HR);
    strSPO2 = String(adcData.SPO2);
    HTML = "<!DOCTYPE html>\
      <head>\
          <title>ESP32 Sensor Kit</title>\
      </head>\
      <body>\
      <body>\
      <div style='text-align: center;background-color: darkgrey;border: 2px solid black;color: #030101;font-size:xx-large'>ESP32 Sensor Kit</div>\
      <div style='text-align: center;background-color: darkgrey;border: 2px solid black;color: #8b1f1b;font-size:x-large'>"
           + strHR + "</div>\
      <div style='text-align: center;background-color: darkgrey;border: 2px solid black;color: #008b61;font-size:x-large'>"
           + strSPO2 + "</div>\
      </body>\
      </body>\
      </html>";
    server.handleClient();
    delay(700);
  }
}

/*
 * HTML Data
 */
void handle_root() {  // Handle root url (/)
  server.send(200, "text/html", HTML);
}

/*
 * 
 * SD Card->Function
 * 
 */
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);
  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}
void appendFile(fs::FS &fs, const char *path, const char *message) {
#ifdef USART_DEBUG
  Serial.printf("Appending to file: %s\n", path);
#endif
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
#ifdef USART_DEBUG
    Serial.println("Message appended");
#endif
  } else {
    Serial.println("Append failed");
  }
  file.close();
}