#include "interval.h"
#include "hoja_includes.h"
#include "main.h"

// LSM6DSR REGISTERS
#define FUNC_CFG_ACCESS 0x01
#define CTRL1_XL        0x10
#define CTRL2_G         0x11  // Gyro Activation Register - Write 0x60 to enable 416hz
#define CTRL3_C         0x12  // Used to set BDU
#define CTRL4_C         0x13
#define CTRL6_C         0x15
#define CTRL8_XL        0x17
#define CTRL9_XL        0x18
#define CTRL10_C        0x19

#define FUNC_MASK   (0b10000000) // Enable FUNC CFG access
#define CTRL1_MASK  (0b10101110) // 1.66kHz, 8G, output first stage filtering
#define CTRL2_MASK  (0b01011100) // 208Hz, 2000dps
#define CTRL3_MASK  (0b00000100) // BDU enabled and Interrupt out active low
#define CTRL4_MASK  (0b00000100) // I2C disable (Check later for LPF for gyro)
#define CTRL6_MASK  (0b00000000) // 12.2 LPF gyro
#define CTRL8_MASK  (0b11100000) //H P_SLOPE_XL_EN
#define CTRL9_MASK  (0x38)
#define CTRL10_MASK (0x38 | 0x4)

#define SPI_READ_BIT 0x80

#define IMU_OUTX_L_G 0x22
#define IMU_OUTX_L_X 0x28

int16_t _app_imu_concat_16(uint8_t low, uint8_t high)
{
  return (int16_t) ((high<<8) | low);
}

void _app_imu_write_register(const uint8_t reg, const uint8_t data)
{
  gpio_put(PGPIO_IMU0_CS, false);
  const uint8_t dat[2] = {reg, data};
  spi_write_blocking(spi0, dat, 2);
  gpio_put(PGPIO_IMU0_CS, true);
  sleep_ms(2);

  gpio_put(PGPIO_IMU1_CS, false);
  spi_write_blocking(spi0, dat, 2);
  gpio_put(PGPIO_IMU1_CS, true);
  sleep_ms(2);
}

void cb_hoja_read_imu(imu_data_s *data_a, imu_data_s *data_b)
{
    uint8_t i[12] = {0};
    const uint8_t reg = 0x80 | IMU_OUTX_L_G;

    gpio_put(PGPIO_IMU0_CS, false);
    spi_write_blocking(spi0, &reg, 1);
    spi_read_blocking(spi0, 0, &i[0], 12);
    gpio_put(PGPIO_IMU0_CS, true);

    data_a->gx = -_app_imu_concat_16(i[0], i[1]);
    data_a->gy = _app_imu_concat_16(i[2], i[3]);
    data_a->gz = _app_imu_concat_16(i[4], i[5]);

    data_a->ax = -_app_imu_concat_16(i[6], i[7]);
    data_a->ay = _app_imu_concat_16(i[8], i[9]);
    data_a->az = _app_imu_concat_16(i[10], i[11]);

    gpio_put(PGPIO_IMU1_CS, false);
    spi_write_blocking(spi0, &reg, 1);
    spi_read_blocking(spi0, 0, &i[0], 12);
    gpio_put(PGPIO_IMU1_CS, true);

    data_b->gx = _app_imu_concat_16(i[0], i[1]);
    data_b->gy = -_app_imu_concat_16(i[2], i[3]);
    data_b->gz = _app_imu_concat_16(i[4], i[5]);

    data_b->ax = _app_imu_concat_16(i[6], i[7]);
    data_b->ay = -_app_imu_concat_16(i[8], i[9]);
    data_b->az = _app_imu_concat_16(i[10], i[11]);

    data_a->retrieved = true;
    data_b->retrieved = true;
}

void app_imu_init()
{
  _app_imu_write_register(CTRL1_XL, CTRL1_MASK);
  _app_imu_write_register(CTRL2_G, CTRL2_MASK);
  _app_imu_write_register(CTRL3_C, CTRL3_MASK);
  _app_imu_write_register(CTRL4_C, CTRL4_MASK);
  _app_imu_write_register(CTRL6_C, CTRL6_MASK);
  _app_imu_write_register(CTRL8_XL, CTRL8_MASK);
}