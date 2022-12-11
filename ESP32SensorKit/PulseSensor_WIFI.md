---
sort: 7
---

# PulseSensor_WIFI

```
本程序相较于PulseSensor.ino增加了WIFI-STA接入功能
此前操作步骤请参考PulseSensor文档
```

<!-- more -->

1. 使用手机创建热点，名称”ESP32 Sensor Kit“，密码“12345678”

2. 重启开发板，在串口监视器中查看连接状态，并记录下开发板IP地址

    <img decoding="async" src="https://addison-cq.github.io/ESP32SensorKit/images/Snipaste_2022-11-11_13-48-55.png" width="40%">

3. 打开ESP32 Sensor Kit应用，点击WIFI接入

    <img decoding="async" src="https://addison-cq.github.io/ESP32SensorKit/images/Screenshot_20221111_123302_com.example.esp32sensorkit_f.jpg" width="20%">

4. 在IP地址栏内填入开发板IP地址

5. 点击连接，上方窗口将出现ESP32 Sensor Kit字样

6. APP会每两秒自动同步一次开发板数据

    <img decoding="async" src="https://addison-cq.github.io/ESP32SensorKit/images/Screenshot_20221111_143601_com.example.esp32sensorkit_f.jpg" width="20%">

   

