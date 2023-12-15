#define X_AXIS_CONFIG 0xD0
#define Y_AXIS_CONFIG 0xF0
#define BUFFER_TO_UINT16(buffer) (uint16_t)(((buffer[0] & 0x07) << 9) | buffer[1] << 1 | buffer[2] >> 7)

#define PGPIO_BUTTON_MODE 5

#define PGPIO_PULL_A    14
#define PGPIO_PULL_B    15
#define PGPIO_PULL_C    16
#define PGPIO_PULL_D    17

#define PGPIO_SCAN_A    13
#define PGPIO_SCAN_B    12
#define PGPIO_SCAN_C    11
#define PGPIO_SCAN_D    10

// Analog L Trigger ADC
#define PADC_LT 3
// Analog R Trigger ADC
#define PADC_RT 1

// Analog L Trigger GPIO
#define PGPIO_LT 27
// Analog R Trigger GPIO
#define PGPIO_RT 26

#define PGPIO_RUMBLE_MAIN   3
#define PGPIO_RUMBLE_BRAKE  8

// SPI ADC CLK pin
  #define PGPIO_SPI_CLK 6
  // SPI ADC TX pin
  #define PGPIO_SPI_TX  7
  // SPI ADC RX pin
  #define PGPIO_SPI_RX  4

  // Left stick ADC Chip Select
  #define PGPIO_LS_CS   1
  // Right stick ADC Chip Select
  #define PGPIO_RS_CS   9

  #define PGPIO_IMU0_CS 0
  #define PGPIO_IMU1_CS 2

  #define PGPIO_BUTTON_USB_SEL 25
  #define PGPIO_BUTTON_USB_EN 24
  #define PGPIO_ESP_EN 26


void _gpio_put_od(uint gpio, bool level);
  