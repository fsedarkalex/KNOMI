#ifndef PINOUT_KNOMI_V2_H
#define PINOUT_KNOMI_V2_H

//Modification by Engr. Jerome Ballad, ECE

#define BOOT_PIN     0

// common i2c
#define I2C0_SUPPORT
#define I2C0_SPEED   100000
#define I2C0_SCL_PIN 1
#define I2C0_SDA_PIN 2
 #define I2C1_SPEED   100000
 #define I2C1_SCL_PIN 3
 #define I2C1_SDA_PIN 4

// GC9A01 SPI TFT
// #define GC9A01_MISO_PIN 13
#define GC9A01_MOSI_PIN 14
#define GC9A01_SCLK_PIN 18
#define GC9A01_CS_PIN   20
#define GC9A01_DC_PIN   19
#define GC9A01_RST_PIN  21
// PWM
#define LCD_BL_PIN         12

// CST816S Touch
#define CST816S_SUPPORT
#define CST816S_IRQ_PIN 17
#define CST816S_RST_PIN 16

// LIS2DW Accelerometer
#define LIS2DW_SUPPORT

// OV2640 Camera
#define PWDN_GPIO_NUM -1 
#define RESET_GPIO_NUM -1 
#define XCLK_GPIO_NUM 7 
#define SIOD_GPIO_NUM I2C1_SDA_PIN
#define SIOC_GPIO_NUM I2C1_SCL_PIN

#define Y2_GPIO_NUM 38 
#define Y3_GPIO_NUM 40 
#define Y4_GPIO_NUM 41 
#define Y5_GPIO_NUM 39 
#define Y6_GPIO_NUM 47 
#define Y7_GPIO_NUM 5  
#define Y8_GPIO_NUM 6  
#define Y9_GPIO_NUM 8  

#define VSYNC_GPIO_NUM 15 
#define HREF_GPIO_NUM 9
#define PCLK_GPIO_NUM 48

// Button
#define BTN_1_PIN I2C1_SDA_PIN
#define BTN_2_PIN I2C1_SCL_PIN
#define BTN_3_PIN CAM_RESET_PIN
// LED
#define LED_2_PIN CAM_D0_PIN
#define LED_3_PIN CAM_D1_PIN
#define LED_4_PIN CAM_D4_PIN

#endif
