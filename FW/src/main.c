#include "hoja_includes.h"
#include "app_rumble.h"
#include "app_imu.h"
#include "main.h"

button_remap_s user_map = {
    .dpad_up = MAPCODE_DUP,
    .dpad_down = MAPCODE_DDOWN,
    .dpad_left = MAPCODE_DLEFT,
    .dpad_right = MAPCODE_DRIGHT,

    .button_a = MAPCODE_B_A,
    .button_b = MAPCODE_B_B,
    .button_x = MAPCODE_B_X,
    .button_y = MAPCODE_B_Y,

    .trigger_l = MAPCODE_T_ZL,
    .trigger_r = MAPCODE_T_ZR,
    .trigger_zl = MAPCODE_T_L,
    .trigger_zr = MAPCODE_T_R,

    .button_plus = MAPCODE_B_PLUS,
    .button_minus = MAPCODE_B_MINUS,
    .button_stick_left = MAPCODE_B_STICKL,
    .button_stick_right = MAPCODE_B_STICKR,
};

uint main_slice_num = 0;
uint brake_slice_num = 0;

void _setup_gpio_pull(uint8_t gpio)
{
  gpio_init(gpio);
  gpio_pull_up(gpio);
  gpio_set_dir(gpio, GPIO_OUT);
  gpio_put(gpio, true);
}

void _setup_gpio_scan(uint8_t gpio)
{
  gpio_init(gpio);
  gpio_pull_up(gpio);
  gpio_set_dir(gpio, GPIO_IN);
  // printf("Set up GPIO: %d", (uint8_t) gpio);
}

void _gpio_put_od(uint gpio, bool level)
{
    if(level)
    {
        gpio_set_dir(gpio, GPIO_IN);
        gpio_pull_up(gpio);
        gpio_put(gpio, 1);
    }
    else
    {
        gpio_set_dir(gpio, GPIO_OUT);
        gpio_disable_pulls(gpio);
        gpio_put(gpio, 0);
    }
}

void cb_hoja_set_uart_enabled(bool enable)
{
    if(enable)
    {
        gpio_put(PGPIO_BUTTON_USB_EN, 1);
        sleep_ms(100);
        gpio_put(PGPIO_BUTTON_USB_SEL, 1);
        sleep_ms(100);
        gpio_put(PGPIO_BUTTON_USB_EN, 0);
    }
    else
    {
        gpio_put(PGPIO_BUTTON_USB_EN, 1);
        sleep_ms(100);
        gpio_put(PGPIO_BUTTON_USB_SEL, 0);
        sleep_ms(100);
        gpio_put(PGPIO_BUTTON_USB_EN, 0);
    }
}

void cb_hoja_set_bluetooth_enabled(bool enable)
{
    if(enable)
    {
        cb_hoja_set_uart_enabled(true);
        // Release ESP to be controlled externally
        _gpio_put_od(PGPIO_ESP_EN, true);
    }
    else
    {
        _gpio_put_od(PGPIO_ESP_EN, false);
    }
}

void cb_hoja_hardware_setup()
{
    // Set up GPIO for input buttons
    _setup_gpio_pull(PGPIO_PULL_A);
    _setup_gpio_pull(PGPIO_PULL_B);
    _setup_gpio_pull(PGPIO_PULL_C);
    _setup_gpio_pull(PGPIO_PULL_D);

    _setup_gpio_scan(PGPIO_SCAN_A);
    _setup_gpio_scan(PGPIO_SCAN_B);
    _setup_gpio_scan(PGPIO_SCAN_C);
    _setup_gpio_scan(PGPIO_SCAN_D);

    // Set up Rumble GPIO
    gpio_init(PGPIO_RUMBLE_MAIN);
    gpio_init(PGPIO_RUMBLE_BRAKE);

    gpio_set_dir(PGPIO_RUMBLE_MAIN, GPIO_OUT);
    gpio_set_dir(PGPIO_RUMBLE_BRAKE, GPIO_OUT);

    gpio_set_function(PGPIO_RUMBLE_MAIN, GPIO_FUNC_PWM);
    gpio_set_function(PGPIO_RUMBLE_BRAKE, GPIO_FUNC_PWM);

    main_slice_num = pwm_gpio_to_slice_num(PGPIO_RUMBLE_MAIN);
    brake_slice_num = pwm_gpio_to_slice_num(PGPIO_RUMBLE_BRAKE);

    pwm_set_wrap(main_slice_num, 255);
    pwm_set_wrap(brake_slice_num, 255);

    pwm_set_chan_level(main_slice_num, PWM_CHAN_B, 0);    // B for odd pins
    pwm_set_chan_level(brake_slice_num, PWM_CHAN_B, 255); // B for odd pins

    pwm_set_enabled(main_slice_num, true);
    pwm_set_enabled(brake_slice_num, true);

    pwm_set_gpio_level(PGPIO_RUMBLE_BRAKE, 255);
    pwm_set_gpio_level(PGPIO_RUMBLE_MAIN, 0);

    sleep_us(150); // Stabilize voltages

    // initialize SPI at 1 MHz
    // initialize SPI at 3 MHz just to test
    spi_init(spi0, 3000 * 1000);
    gpio_set_function(PGPIO_SPI_CLK, GPIO_FUNC_SPI);
    gpio_set_function(PGPIO_SPI_TX, GPIO_FUNC_SPI);
    gpio_set_function(PGPIO_SPI_RX, GPIO_FUNC_SPI);

    // IMU 0 initialize
    gpio_init(PGPIO_IMU0_CS);
    gpio_set_dir(PGPIO_IMU0_CS, GPIO_OUT);
    gpio_put(PGPIO_IMU0_CS, true); // active low

    // IMU 1 initialize
    gpio_init(PGPIO_IMU1_CS);
    gpio_set_dir(PGPIO_IMU1_CS, GPIO_OUT);
    gpio_put(PGPIO_IMU1_CS, true); // active low

    app_imu_init();
}

bool set = false;
bool unset = true;

void cb_hoja_read_buttons(button_data_s *data)
{
    // Keypad version
    gpio_put(PGPIO_PULL_A, false);
    sleep_us(5);
    data->button_a  = !gpio_get(PGPIO_SCAN_A);
    data->button_b  = !gpio_get(PGPIO_SCAN_B);
    data->button_x  = !gpio_get(PGPIO_SCAN_C);
    data->button_y  = !gpio_get(PGPIO_SCAN_D);
    gpio_put(PGPIO_PULL_A, true);

    gpio_put(PGPIO_PULL_B, false);
    sleep_us(5);
    data->dpad_up       = !gpio_get(PGPIO_SCAN_A);
    data->dpad_left     = !gpio_get(PGPIO_SCAN_B);
    data->dpad_down     = !gpio_get(PGPIO_SCAN_C);
    data->dpad_right    = !gpio_get(PGPIO_SCAN_D);
    gpio_put(PGPIO_PULL_B, true);

    gpio_put(PGPIO_PULL_C, false);
    sleep_us(5);
    data->trigger_l     = !gpio_get(PGPIO_SCAN_A);
    data->trigger_zl    = !gpio_get(PGPIO_SCAN_B);
    data->trigger_r     = !gpio_get(PGPIO_SCAN_C);
    data->trigger_zr    = !gpio_get(PGPIO_SCAN_D);
    gpio_put(PGPIO_PULL_C, true);

    gpio_put(PGPIO_PULL_D, false);
    sleep_us(5);
    data->button_stick_left     = !gpio_get(PGPIO_SCAN_A);
    data->button_plus           = !gpio_get(PGPIO_SCAN_B);
    data->button_stick_right    = !gpio_get(PGPIO_SCAN_C);
    gpio_put(PGPIO_PULL_D, true);

    data->button_shipping = !gpio_get(PGPIO_BUTTON_MODE);
    data->button_sync = data->button_plus;
}

void cb_hoja_read_analog(a_data_s *data)
{
    // Set up buffers for each axis
    uint8_t buffer_lx[3] = {0};
    uint8_t buffer_ly[3] = {0};
    uint8_t buffer_rx[3] = {0};
    uint8_t buffer_ry[3] = {0};

    // CS left stick ADC
    gpio_put(PGPIO_LS_CS, false);
    // Read first axis for left stick
    spi_read_blocking(spi0, X_AXIS_CONFIG, buffer_lx, 3);

    // CS left stick ADC reset
    gpio_put(PGPIO_LS_CS, true);
    gpio_put(PGPIO_LS_CS, false);

    // Set up and read axis for left stick Y  axis
    spi_read_blocking(spi0, Y_AXIS_CONFIG, buffer_ly, 3);

    // CS right stick ADC
    gpio_put(PGPIO_LS_CS, true);
    gpio_put(PGPIO_RS_CS, false);

    spi_read_blocking(spi0, Y_AXIS_CONFIG, buffer_ry, 3);

    // CS right stick ADC reset
    gpio_put(PGPIO_RS_CS, true);
    gpio_put(PGPIO_RS_CS, false);

    spi_read_blocking(spi0, X_AXIS_CONFIG, buffer_rx, 3);

    // Release right stick CS ADC
    gpio_put(PGPIO_RS_CS, true);

    // Convert data
    data->lx = BUFFER_TO_UINT16(buffer_lx);
    data->ly = BUFFER_TO_UINT16(buffer_ly);
    data->rx = BUFFER_TO_UINT16(buffer_rx);
    data->ry = BUFFER_TO_UINT16(buffer_ry);
}

void cb_hoja_task_1_hook(uint32_t timestamp)
{
    app_rumble_task(timestamp);
}

int main()
{
    stdio_init_all();
    sleep_ms(100);

    printf("GC Ultimate Started.\n");

    cb_hoja_hardware_setup();

    gpio_init(PGPIO_ESP_EN);
    cb_hoja_set_bluetooth_enabled(false);

    gpio_init(PGPIO_BUTTON_USB_EN);
    gpio_set_dir(PGPIO_BUTTON_USB_EN, GPIO_OUT);
    gpio_put(PGPIO_BUTTON_USB_EN, 0);

    gpio_init(PGPIO_BUTTON_USB_SEL);
    gpio_set_dir(PGPIO_BUTTON_USB_SEL, GPIO_OUT);
    gpio_put(PGPIO_BUTTON_USB_SEL, 0);

    gpio_init(PGPIO_BUTTON_MODE);
    gpio_set_dir(PGPIO_BUTTON_MODE, GPIO_IN);
    gpio_pull_up(PGPIO_BUTTON_MODE);

    button_data_s tmp = {0};
    cb_hoja_read_buttons(&tmp);

    hoja_config_t _config = {
            .input_method   = INPUT_METHOD_AUTO,
            .input_mode     = INPUT_MODE_LOAD,
        };

    if(tmp.button_plus && !tmp.trigger_r)
    {
        reset_usb_boot(0, 0);
    }
    else if (tmp.trigger_r && tmp.button_plus)
    {
        _config.input_method = INPUT_METHOD_BLUETOOTH;
        // Release ESP to be controlled externally
        cb_hoja_set_bluetooth_enabled(true);
        cb_hoja_set_uart_enabled(true);

        sleep_ms(3500);

        for(;;)
        {
            cb_hoja_read_buttons(&tmp);
            if(tmp.trigger_l)
            {
                watchdog_reboot(0, 0, 0);
            }

            sleep_ms(150);
        }
    }

    hoja_init(&_config);
}
