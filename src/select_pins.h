// Camera
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   18
#define XCLK_GPIO_NUM    14
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5

#define Y9_GPIO_NUM      15
#define Y8_GPIO_NUM      16
#define Y7_GPIO_NUM      17
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM      11
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM    13

// Power / PCIe (Modem pins)
#define PWR_ON_PIN       1
#define PCIE_PWR_PIN     48
#define PCIE_RST_PIN     48
#define PCIE_TX_PIN      45
#define PCIE_RX_PIN      46
#define PCIE_LED_PIN     21

// Modem configuration
#define MODEM_BAUDRATE              115200
#define MODEM_START_WAIT_MS         3000
#define MODEM_PWRON_PWMS            100
#define MODEM_PWROFF_PWMS   3000
#define MODEM_RESET_LEVEL           HIGH

// Serial port for modem AT commands
#define SerialAT                    Serial1

// GPS configuration
#define MODEM_GPS_ENABLE_GPIO       4
#define MODEM_GPS_ENABLE_LEVEL      1

// LED indicator
#define BOARD_LED_PIN               21
#define LED_ON                      HIGH

// SD
#define SD_MISO_PIN      40
#define SD_MOSI_PIN      38
#define SD_SCLK_PIN      39
#define SD_CS_PIN        47

// Microphone
#define MIC_IIS_WS_PIN   42
#define MIC_IIS_SCK_PIN  41
#define MIC_IIS_DATA_PIN 2

// Other
#define BUTTON_PIN       0
#define BAT_VOLT_PIN     -1

// Groove to MCP23017 I2C Pins
#define SDA_PIN 43      // Grove RX (cerveny kábel)
#define SCL_PIN 44      // Grove TX (biely kábel)

// MCP23017 Pins
#define MCP_ADDRESS 0x20

#define MCP_PIR_PIN 8   // GPB0 na MCP23017

#define OUTPUT1Y   0
#define OUTPUT2Y   1
#define OUTPUT3Y   2
#define OUTPUT4Y   3

#define OUTPUT1X    4
#define OUTPUT2X    5
#define OUTPUT3X    6
#define OUTPUT4X    7

#define DELAY 2 

//WIFI
#define WIFI_AP_SSID     "T-SIMCAM-"
#define WIFI_AP_PASSWORD "12345678"

// Compatibility: if other code uses CAM_* names, map them to the standard names
#ifndef CAM_PWDN_PIN
#define CAM_PWDN_PIN     PWDN_GPIO_NUM
#endif
#ifndef CAM_RESET_PIN
#define CAM_RESET_PIN    RESET_GPIO_NUM
#endif
#ifndef CAM_XCLK_PIN
#define CAM_XCLK_PIN     XCLK_GPIO_NUM
#endif
#ifndef CAM_SIOD_PIN
#define CAM_SIOD_PIN     SIOD_GPIO_NUM
#endif
#ifndef CAM_SIOC_PIN
#define CAM_SIOC_PIN     SIOC_GPIO_NUM
#endif
#ifndef CAM_Y9_PIN
#define CAM_Y9_PIN       Y9_GPIO_NUM
#endif
#ifndef CAM_Y8_PIN
#define CAM_Y8_PIN       Y8_GPIO_NUM
#endif
#ifndef CAM_Y7_PIN
#define CAM_Y7_PIN       Y7_GPIO_NUM
#endif
#ifndef CAM_Y6_PIN
#define CAM_Y6_PIN       Y6_GPIO_NUM
#endif
#ifndef CAM_Y5_PIN
#define CAM_Y5_PIN       Y5_GPIO_NUM
#endif
#ifndef CAM_Y4_PIN
#define CAM_Y4_PIN       Y4_GPIO_NUM
#endif
#ifndef CAM_Y3_PIN
#define CAM_Y3_PIN       Y3_GPIO_NUM
#endif
#ifndef CAM_Y2_PIN
#define CAM_Y2_PIN       Y2_GPIO_NUM
#endif
#ifndef CAM_VSYNC_PIN
#define CAM_VSYNC_PIN    VSYNC_GPIO_NUM
#endif
#ifndef CAM_HREF_PIN
#define CAM_HREF_PIN     HREF_GPIO_NUM
#endif
#ifndef CAM_PCLK_PIN
#define CAM_PCLK_PIN     PCLK_GPIO_NUM
#endif
