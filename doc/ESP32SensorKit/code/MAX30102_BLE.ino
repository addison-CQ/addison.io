#include "esp32-hal-cpu.h"
#include <stdio.h>
#include <string.h>
int initFlag = 0;  //硬件初始化标志
int intFlag = 0;   //按键1中断标志位
int count = 0;
char point[11];
#define USART_DEBUG 1  //使能串口打印调试信息
//RTC
#include <ESP32Time.h>
ESP32Time rtc(0);  //时间偏移量，单位ms
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
//BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
char BLEbuf[32] = { 0 };
uint32_t cnt = 0;
//Serial1->MAX30102
int flag = 0;
char ch;
struct Data {
  char HR[10];
  char SPO2[10];
} uartData;
//OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /*reset=*/U8X8_PIN_NONE, /*clock=*/OLED_SCL, /*data=*/OLED_SDA);  //U8G2程序库 SSD1306控制晶片 128X64_NONAME解析度和型号 F暂存区大小可以改为1或2 F_HW_I2C控制介面 u8g2(U8G2_R0代表不旋转,U8X8_PIN_NONE代表没有重置引脚)
//BLE
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};
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
  //使能按键1中断监测
  attachInterrupt(4, INTfunction, FALLING);
  /*
   * RTC
   */
  rtc.setTime(0, 0, 14, 7, 11, 2022);  // 7th Nov 2021 14:0:0
  /*
   * OLED
   */
  u8g2.setI2CAddress(0x78);
  u8g2.begin();
  u8g2.enableUTF8Print();  //使能UTF-8支持库
  u8g2.setFont(u8g2_font_unifont_t_symbols);
  u8g2.clear();
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
   * UART->MAX30102
   */
  Serial1.begin(9600, SERIAL_8N1, /*rx =*/14, /*Tx =*/12);
  Serial1.print("STOP\r\n");
  delay(50);
  while (Serial1.read() >= 0) {};  //清空串口缓存
  Serial1.print("START\r\n");
  delay(50);
  if (Serial1.read() != 79) {
    u8g2.clearBuffer();
    u8g2.firstPage();
    do {
      u8g2.setCursor(0, 14);           //指定显示位置
      u8g2.print("ESP32 Sensor Kit");  //使用print来显示字符串
      u8g2.setCursor(0, 35);           //指定显示位置
      u8g2.print("MAX3010 Start");     //使用print来显示字符串
      u8g2.setCursor(0, 55);           //指定显示位置
      u8g2.print("Failed !!!");        //使用print来显示字符串
    } while (u8g2.nextPage());
    Serial.println("MAX3010 Start Failed !");
    return;
  }
  delay(50);
  while (Serial1.read() >= 0) {};  //清空串口缓存
  /*
   * BLE
   */
  BLEDevice::init("ESP32 Sensor Kit");             // Create the BLE Device
  BLEServer *pServer = BLEDevice::createServer();  // Create the BLE Server
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);                                                   // Create the BLE Service
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);  // Create a BLE Characteristic
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();                   // Start the service
  pServer->getAdvertising()->start();  // Start advertising
  Serial.println("Waiting a client connection to notify...");
  //使能按键1中断监测
  attachInterrupt(4, INTfunction, FALLING);
  initFlag = 1;  //硬件初始化成功 标志置一
}

void loop() {
  if (initFlag == 0) {  //初始化失败则退出程序
    Serial1.print("STOP\r\n");
    delay(10);
    Serial.print("Hardware Init Failed !\r\n");
    delay(3000);
    ESP.restart();
  }
  /*
   * UART->MAX30102
   */
  flag = 0;
  Serial1.print("HR\r\n");
  delay(50);
  while (Serial1.available()) {
    ch = Serial1.read();
    if (ch != 13 && ch != 10 && ch != 0) {
      uartData.HR[flag] = ch;
      flag++;
    }
#ifdef USART_DEBUG
    Serial.print(ch);
#endif
  }
  flag = 0;
  Serial1.print("SPO2\r\n");
  delay(50);
  while (Serial1.available()) {
    ch = Serial1.read();
    if (ch != 13 && ch != 10 && ch != 0) {
      uartData.SPO2[flag] = ch;
      flag++;
    }
#ifdef USART_DEBUG
    Serial.print(ch);
#endif
  }
  /*
   * OLED
   */
  u8g2.clearBuffer();
  u8g2.firstPage();
  do {
    if (uartData.HR[0] == 'N' || uartData.SPO2[0] == 'N') {
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
      u8g2.print(uartData.HR);         //使用print来显示字符串
      if (intFlag == 1) {
        u8g2.setCursor(100, 40);  //指定显示位置
        u8g2.print("REC");        //使用print来显示字符串
      }
      u8g2.setCursor(0, 55);      //指定显示位置
      u8g2.print(uartData.SPO2);  //使用print来显示字符串
    }
  } while (u8g2.nextPage());
  /*
   * BLE
   */
  if (deviceConnected) {
    pCharacteristic->setValue(uartData.SPO2);
    pCharacteristic->notify();  // Send the value to the app
    pCharacteristic->setValue(uartData.HR);
    pCharacteristic->notify();  // Send the value to the app
  }
  /*
   * SD Card
   */
  if (intFlag == 1) {
    if (uartData.HR[0] == 'H' && uartData.HR[1] == 'R') {
      appendFile(SD, "/MAX30102_BLE.txt", &(rtc.getTime("%B %d %Y %H:%M:%S"))[0]);
      appendFile(SD, "/MAX30102_BLE.txt", "\t\t");
      appendFile(SD, "/MAX30102_BLE.txt", uartData.HR);
      appendFile(SD, "/MAX30102_BLE.txt", "\r\n");
    }
    if (uartData.SPO2[0] == 'S' && uartData.SPO2[1] == 'P' && uartData.SPO2[2] == 'O' && uartData.SPO2[3] == '2') {
      appendFile(SD, "/MAX30102_BLE.txt", &(rtc.getTime("%B %d %Y %H:%M:%S"))[0]);
      appendFile(SD, "/MAX30102_BLE.txt", "\t\t");
      appendFile(SD, "/MAX30102_BLE.txt", uartData.SPO2);
      appendFile(SD, "/MAX30102_BLE.txt", "\r\n");
    }
  }
  memset(uartData.HR, 0, sizeof(char) * 10);
  memset(uartData.SPO2, 0, sizeof(char) * 10);
  delay(200);
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