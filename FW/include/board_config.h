#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#define HOJA_BT_LOGGING_DEBUG 0

// Device stuff

//#define HOJA_DEVICE_ID  0xC001

#if (HOJA_DEVICE_ID == 0xC001) // GC Ultimate Prototype
#define GC_ULT_TYPE 0
#elif (HOJA_DEVICE_ID == 0xC002) // GC Ultimate R4
#define GC_ULT_TYPE 1
#endif

#define HOJA_FW_VERSION 0x0A0C

#if (GC_ULT_TYPE == 0)

// RGB Stuff
#define HOJA_RGB_PIN 21
#define HOJA_RGB_COUNT 4
#define HOJA_RGBW_EN 0

// GPIO definitions
#define HOJA_SERIAL_PIN     19
#define HOJA_CLOCK_PIN      20
#define HOJA_LATCH_PIN      18

// URL that will display to open a config tool
#define HOJA_WEBUSB_URL     "handheldlegend.github.io/hoja_config/"
#define HOJA_MANUFACTURER   "HHL"
#define HOJA_PRODUCT        "GCU Prototype"

#define HOJA_CAPABILITY_ANALOG_STICK_L 1
#define HOJA_CAPABILITY_ANALOG_STICK_R 1
#define HOJA_CAPABILITY_ANALOG_TRIGGER_L 1
#define HOJA_CAPABILITY_ANALOG_TRIGGER_R 1

#define HOJA_CAPABILITY_BLUETOOTH 1
#define HOJA_CAPABILITY_BATTERY 1
#define HOJA_CAPABILITY_RGB 1
#define HOJA_CAPABILITY_GYRO 1

#define HOJA_CAPABILITY_NINTENDO_SERIAL 1
#define HOJA_CAPABILITY_NINTENDO_JOYBUS 1

#define HOJA_CAPABILITY_RUMBLE_ERM 1
#define HOJA_CAPABILITY_RUMBLE_LRA 0

#define HOJA_RGB_GROUP_RS       {-1}
#define HOJA_RGB_GROUP_LS       {0,1,2,3}
#define HOJA_RGB_GROUP_DPAD     {-1}
#define HOJA_RGB_GROUP_MINUS    {-1}
#define HOJA_RGB_GROUP_CAPTURE  {-1}
#define HOJA_RGB_GROUP_HOME     {-1}
#define HOJA_RGB_GROUP_PLUS     {-1}
#define HOJA_RGB_GROUP_Y        {-1}
#define HOJA_RGB_GROUP_X        {-1}
#define HOJA_RGB_GROUP_A        {-1}
#define HOJA_RGB_GROUP_B        {-1}
#define HOJA_RGB_GROUP_L        {-1}
#define HOJA_RGB_GROUP_R        {-1}
#define HOJA_RGB_GROUP_ZR       {-1}
#define HOJA_RGB_GROUP_ZL       {-1}
#define HOJA_RGB_GROUP_PLAYER   {-1}

#define HOJA_I2C_BUS i2c1
#define HOJA_I2CINPUT_ADDRESS 0x76
#define HOJA_I2C_SDA 22
#define HOJA_I2C_SCL 23

#elif (GC_ULT_TYPE == 1)

// RGB Stuff
#define HOJA_RGB_PIN 27
#define HOJA_RGB_COUNT 12
#define HOJA_RGBW_EN 0

// GPIO definitions
#define HOJA_SERIAL_PIN     20
#define HOJA_CLOCK_PIN      21
#define HOJA_LATCH_PIN      19

// URL that will display to open a config tool
#define HOJA_WEBUSB_URL     "handheldlegend.github.io/hoja_config/"
#define HOJA_MANUFACTURER   "HHL"
#define HOJA_PRODUCT        "GCU R4"

#define HOJA_CAPABILITY_ANALOG_STICK_L 1
#define HOJA_CAPABILITY_ANALOG_STICK_R 1
#define HOJA_CAPABILITY_ANALOG_TRIGGER_L 1
#define HOJA_CAPABILITY_ANALOG_TRIGGER_R 1

#define HOJA_CAPABILITY_BLUETOOTH 1
#define HOJA_CAPABILITY_BATTERY 1
#define HOJA_CAPABILITY_RGB 1
#define HOJA_CAPABILITY_GYRO 1

#define HOJA_CAPABILITY_NINTENDO_SERIAL 1
#define HOJA_CAPABILITY_NINTENDO_JOYBUS 1

#define HOJA_CAPABILITY_RUMBLE_ERM 0
#define HOJA_CAPABILITY_RUMBLE_LRA 1

#define HOJA_RGB_GROUP_RS       {-1}
#define HOJA_RGB_GROUP_LS       {0,1,2,3}
#define HOJA_RGB_GROUP_DPAD     {-1}
#define HOJA_RGB_GROUP_MINUS    {-1}
#define HOJA_RGB_GROUP_CAPTURE  {-1}
#define HOJA_RGB_GROUP_HOME     {-1}
#define HOJA_RGB_GROUP_PLUS     {-1}
#define HOJA_RGB_GROUP_Y        {-1}
#define HOJA_RGB_GROUP_X        {-1}
#define HOJA_RGB_GROUP_A        {-1}
#define HOJA_RGB_GROUP_B        {-1}
#define HOJA_RGB_GROUP_L        {-1}
#define HOJA_RGB_GROUP_R        {-1}
#define HOJA_RGB_GROUP_ZR       {-1}
#define HOJA_RGB_GROUP_ZL       {-1}
#define HOJA_RGB_GROUP_PLAYER   {4}

#define HOJA_I2C_BUS i2c1
#define HOJA_I2CINPUT_ADDRESS 0x76
#define HOJA_I2C_SDA 22
#define HOJA_I2C_SCL 23

#endif

#define HOJA_ANALOG_HAIRTRIGGER_L 45
#define HOJA_ANALOG_HAIRTRIGGER_R 45

#endif
