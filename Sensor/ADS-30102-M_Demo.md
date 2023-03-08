#ADS-30102-M_Demo

## Demo-1

- 集成开发环境：Arduino IDE
- 开发板：DCcduino（它和Arduino UNO开发板是互相兼容的）

   <img decoding="async" src="https://addison-cq.github.io/webPages/images/demo1-1.png" width="60%">

1. 在Arduino UNO示例SoftwareSerialExample.ino的基础上进行修改，编写一个简单的测试程序

   ```c
   #include <SoftwareSerial.h>
   SoftwareSerial mySerial(10, 11);  // RX, TX
   char ch;
   void setup() {
     // Open serial communications and wait for port to open:
     Serial.begin(115200);
     while (!Serial) {
       ;  // wait for serial port to connect. Needed for native USB port only
     }
     Serial.println("Hello,I'm DCcduino UNO!");
     // set the data rate for the SoftwareSerial port
     mySerial.begin(9600);
     mySerial.print("STOP\r\n");
     delay(50);
     mySerial.print("START\r\n");
     delay(50);
     //清空串口缓存
     while (mySerial.read() >= 0) {};
   }
   void loop() {
     //获取传感器心率数据
     mySerial.print("HR\r\n");
     while (mySerial.available()) {
       ch = mySerial.read();
       Serial.print(ch);
     }
     delay(50);
     //获取传感器血氧饱和度数据
     mySerial.print("SPO2\r\n");
     while (mySerial.available()) {
       ch = mySerial.read();
       Serial.print(ch);
     }
     delay(50);
     delay(1000);
   }
   ```

2. 烧录程序后查看串口监视器信息

   <img decoding="async" src="https://addison-cq.github.io/webPages/images/demo1-2.png" width="80%">

3. 将食指指腹紧贴于传感器模块发出红光的地方，继续观察串口监视器打印出的信息

   <img decoding="async" src="https://addison-cq.github.io/webPages/images/demo1-3.png" width="80%">

4. 在之后的多次测试过程中，模块一直运行正常，检测速度较快，在几秒种内就能测算出心率血氧数据且数据稳定可靠

[工程源码：**UNO_ADS-30102-M.rar**](https://github.com/addison-CQ/webPages/tree/develop/doc/ADS-30102-M)

------

## Demo-2

- 集成开发环境：Keil	HAL库

- 主控芯片：STM32F103C8T6

- 屏幕：1.3寸IPS，分辨率240*240，驱动芯片ST7789，通信接口SPI

  <img decoding="async" src="https://addison-cq.github.io/webPages/images/demo2-1.png" width="60%">

1. 主要代码

   ```c
   int main(void)
   {
     HAL_Init();
   
     /* USER CODE BEGIN Init */
   
     /* USER CODE END Init */
   
     /* Configure the system clock */
     SystemClock_Config();
   
     /* USER CODE BEGIN SysInit */
   
     /* USER CODE END SysInit */
   
     /* Initialize all configured peripherals */
     MX_GPIO_Init();
     MX_USART1_UART_Init();
     /* USER CODE BEGIN 2 */
       LCD_Init(); 
       LCD_Clear(BLACK);
       HAL_UART_Transmit_IT(&huart1, stopBuffer, sizeof(stopBuffer)-1);
       HAL_Delay(50);
       HAL_UART_Transmit_IT(&huart1, startBuffer, sizeof(startBuffer)-1);
       HAL_Delay(50);
       HAL_UART_Receive_IT(&huart1, &rx_data, 1);
     /* USER CODE END 2 */
   
     /* Infinite loop */
     /* USER CODE BEGIN WHILE */
     while (1)
     {
   
           HAL_UART_Transmit_IT(&huart1, spo2Buffer, sizeof(spo2Buffer)-1);
           HAL_Delay(500);
           HAL_UART_Transmit_IT(&huart1, hrBuffer, sizeof(hrBuffer)-1);
           HAL_Delay(500);
           LCD_Clear(BLACK);
       /* USER CODE END WHILE */
        
       /* USER CODE BEGIN 3 */
   
     }
     /* USER CODE END 3 */
   }
   
   /**
   
     * @brief System Clock Configuration
     * @retval None
       */
       void SystemClock_Config(void)
       {
         RCC_OscInitTypeDef RCC_OscInitStruct = {0};
         RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
   
     /** Initializes the RCC Oscillators according to the specified parameters
   
     * in the RCC_OscInitTypeDef structure.
       */
         RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
         RCC_OscInitStruct.HSEState = RCC_HSE_ON;
         RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
         RCC_OscInitStruct.HSIState = RCC_HSI_ON;
         RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
         RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
         RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
         if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
         {
       Error_Handler();
         }
         /** Initializes the CPU, AHB and APB buses clocks
         */
         RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                 |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
         RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
         RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
         RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
         RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
   
     if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
     {
       Error_Handler();
     }
   }
   
   /* USER CODE BEGIN 4 */
   void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
   {
       if(huart->Instance == USART1)
       {        
           receiveBuffer[rx_count]= rx_data;
           rx_count ++;
           if(rx_data == 0x0A)
           {            
               str1 = (char *)receiveBuffer;        //get uart data
               if(strstr(str1, "OK") != NULL)
               {
               
               }
               else if(strstr(str1, "HR") != NULL)
               {
                       for(int i =0;i<=rx_count-3;i++)
                           LCD_ShowChar(100+8*i,100,receiveBuffer[i],1,RED);
               }
               else if(strstr(str1, "SPO2") != NULL)
               {
                       for(int i =0;i<=rx_count-3;i++)
                           LCD_ShowChar(100+8*i,120,receiveBuffer[i],1,GREEN);
               }
               else
               {
                   
               }
               rx_count = 0;
               //clear buff data 
               //清除数组数组，全部赋值为 零
               memset((char *)receiveBuffer, 0, strlen((const char*)receiveBuffer));
        } 
            HAL_UART_Receive_IT(&huart1,&rx_data,1);
       }
   }
   ```

2. 将食指指腹紧贴于MAX30102模块发出红光处，查看屏幕显示效果

   <img decoding="async" src="https://addison-cq.github.io/webPages/images/demo2-2.png" width="60%">

[工程源码：**F103_ADS-30102-M.rar**](https://github.com/addison-CQ/webPages/tree/develop/doc/ADS-30102-M)
