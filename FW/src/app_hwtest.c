#include "app_hwtest.h"
#include "main.h"
#include "app_imu.h"

static bool _hw_test_setup = false;

bool _hwtest_pinok(uint primary)
{
    uint second = 0;
    uint third = 0;
    switch(primary)
    {
        default:
        return false;
        break;

        case HOJA_LATCH_PIN:
            second = HOJA_CLOCK_PIN;
            third = HOJA_SERIAL_PIN;
        break;
        case HOJA_CLOCK_PIN:
            second = HOJA_SERIAL_PIN;
            third = HOJA_LATCH_PIN;
        break;
        case HOJA_SERIAL_PIN:
            second = HOJA_CLOCK_PIN;
            third = HOJA_LATCH_PIN;
        break;
    };

    gpio_init(primary);
    gpio_set_dir(primary, false);
    gpio_pull_up(primary);

    gpio_init(second);
    gpio_set_dir(second, true);
    gpio_put(second, false);

    gpio_init(third);
    gpio_set_dir(third, true);
    gpio_put(third, false);

    bool read = gpio_get(primary);

    // Fail if our pin is read low
    if(!read) return false;

    // Pass if we read high (pulled up)
    return true;
}

bool _hwtest_data()
{
    return _hwtest_pinok(HOJA_SERIAL_PIN);
}

bool _hwtest_latch()
{
    return _hwtest_pinok(HOJA_LATCH_PIN);
}

bool _hwtest_clock()
{
    return _hwtest_pinok(HOJA_CLOCK_PIN);
}

bool _hwtest_bluetooth()
{
    // Release ESP to be controlled externally
    _gpio_put_od(PGPIO_ESP_EN, true);

    sleep_ms(600);

    uint16_t v = btinput_get_version();

    if(v>0) return true;

    return false;
}

bool _hwtest_battery()
{
    return util_battery_comms_check();
}

#define IMU_OUTX_L_G 0x22
#define IMU_OUTX_L_X 0x28
#define IMU_ATTEMPTS 20

bool _hwtest_imu()
{
    uint attempts = IMU_ATTEMPTS;

    while (attempts--)
    {
        uint8_t imu_read[12] = {0};
        const uint8_t reg = 0x80 | IMU_OUTX_L_G;

        bool pass_0 = false;
        while(!spi_is_readable(spi0)){}
        gpio_put(PGPIO_IMU0_CS, false);
        spi_write_blocking(spi0, &reg, 1);
        int read = spi_read_blocking(spi0, 0, &imu_read[0], 12);
        gpio_put(PGPIO_IMU0_CS, true);

        if (read != 12)
            return false;

        for (uint i = 0; i < 12; i++)
        {
            if (imu_read[i] > 0)
                pass_0 = true;
        }

        sleep_ms(24);

        bool pass_1 = false;
        while(!spi_is_readable(spi0)){}
        gpio_put(PGPIO_IMU1_CS, false);
        spi_write_blocking(spi0, &reg, 1);
        read = spi_read_blocking(spi0, 0, &imu_read[0], 12);
        gpio_put(PGPIO_IMU1_CS, true);

        if (read != 12)
            return false;

        for (uint i = 0; i < 12; i++)
        {
            if (imu_read[i] > 0)
                pass_1 = true;
        }

        if (pass_1 && pass_0)
            return true;

        sleep_ms(24);
    }

    return false;
}

#define ANALOG_ATTEMPTS 50
bool _hwtest_analog()
{
    uint attempts = ANALOG_ATTEMPTS;

    // Set up buffers for each axis
    uint8_t buffer_lx[3] = {0};
    uint8_t buffer_ly[3] = {0};
    uint8_t buffer_rx[3] = {0};
    uint8_t buffer_ry[3] = {0};

    while (attempts--)
    {
        // CS left stick ADC
        gpio_put(PGPIO_LS_CS, false);
        // Read first axis for left stick
        while (!spi_is_readable(spi0))
        {
        };
        int read = spi_read_blocking(spi0, X_AXIS_CONFIG, buffer_lx, 3);
        if (read != 3)
            return false;

        // CS left stick ADC reset
        gpio_put(PGPIO_LS_CS, true);
        gpio_put(PGPIO_LS_CS, false);

        while (!spi_is_readable(spi0))
        {
        };
        // Set up and read axis for left stick Y  axis
        read = spi_read_blocking(spi0, Y_AXIS_CONFIG, buffer_ly, 3);
        if (read != 3)
            return false;

        // CS right stick ADC
        gpio_put(PGPIO_LS_CS, true);
        gpio_put(PGPIO_RS_CS, false);

        while (!spi_is_readable(spi0))
        {
        };
        read = spi_read_blocking(spi0, Y_AXIS_CONFIG, buffer_ry, 3);
        if (read != 3)
            return false;

        // CS right stick ADC reset
        gpio_put(PGPIO_RS_CS, true);
        gpio_put(PGPIO_RS_CS, false);

        while (!spi_is_readable(spi0))
        {
        };
        read = spi_read_blocking(spi0, X_AXIS_CONFIG, buffer_rx, 3);
        if (read != 3)
            return false;

        // Release right stick CS ADC
        gpio_put(PGPIO_RS_CS, true);

        bool lx = false;
        bool ly = false;
        bool rx = false;
        bool ry = false;
        for (uint i = 0; i < 3; i++)
        {
            if (buffer_lx[i] > 0)
                lx = true;
            if (buffer_rx[i] > 0)
                rx = true;
            if (buffer_ly[i] > 0)
                ly = true;
            if (buffer_ry[i] > 0)
                ry = true;
        }

        if (lx && ly && rx && ry)
        {
            attempts = 0;
            return true;
        }
    }

    return false;
}

bool _hwtest_rumble()
{
    #if (GC_ULT_TYPE == 0)
    cb_hoja_rumble_test();
    #elif (GC_ULT_TYPE == 1)
    app_rumble_hwtest();
    #endif
    return true;
}

bool _hwtest_rgb()
{
    return true;
}

/**
 * @file app_hwtest.c
 * @brief Hardware test function for the application.
 *
 * This function performs a looped hardware test and returns the test result as a 16-bit unsigned integer.
 * The test includes checking various hardware components such as analog, battery, bluetooth, clock pin, data pin,
 * latch pin, RGB pin, and IMU.
 *
 * @return The test result as a 16-bit unsigned integer.
 */
uint16_t cb_hoja_hardware_test()
{
    hoja_hw_test_u _t;

    _t.analog = _hwtest_analog();
    _t.battery = _hwtest_battery();
    _t.bluetooth = _hwtest_bluetooth();
    _t.clock_pin = _hwtest_clock();
    _t.data_pin = _hwtest_data();
    _t.latch_pin = _hwtest_latch();
    _t.rgb_pin = _hwtest_rgb();
    _t.imu = _hwtest_imu();
    _t.rumble = _hwtest_rumble();

    return _t.val;
}
