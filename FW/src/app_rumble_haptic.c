#include "hoja_includes.h"
#include "interval.h"
#include "main.h"
#include "math.h"
#include "float.h"

#define PWM_CLOCK_BASE 2000000 // 2Mhz
#define PWM_CLOCK_MULTIPLIER 1000 // Scale back up to where we need it

#define B3 246.94f  // Frequency of B3 in Hz
#define E4 329.63f  // Frequency of E4 in Hz
#define A4 440.00f  // Frequency of A4 in Hz
#define D5 587.33f  // Frequency of D5 in Hz
#define Db5 554.37f // Frequency of Db5 in Hz
#define Ab4 415.30f // Frequency of Ab4 in Hz
#define Eb4 311.13f // Frequency of Eb4 in Hz
#define Fs4 369.99f // Frequency of Fs4 in Hz
#define Db4 277.18f // Frequency of Db4 in Hz
#define Ab3 207.65f // Frequency of Ab3 in Hz
#define D4 293.66f  // Frequency of D4 in Hz
#define G4 392.00f  // Frequency of G4 in Hz
#define C5 523.25f  // Frequency of C5 in Hz
#define F5 698.46f  // Frequency of F5 in Hz
#define Bb5 932.33f // Frequency of Bb5 in Hz
#define A5 880.00f  // Frequency of A5 in Hz
#define E5 659.26f  // Frequency of E5 in Hz
#define B4 493.88f  // Frequency of B4 in Hz
#define Ab5 830.61f // Frequency of Ab5 in Hz

float song[28] = {
    B3, E4, A4, E4, D5, E4, Db5, Ab4, Eb4, Ab4, Fs4, Db4, Ab3, Db4, D4, G4, C5, F5, Bb5, F5, C5, A5, E5, B4, Ab5, 0, 0, E4
    };

#define DRV2605_SLAVE_ADDR 0x5A

uint slice_num_hi;
uint slice_num_lo;

float _current_amplitude = 0;
float _current_frequency = 0;

float _rumble_scaler = 0;

/* Note from datasheet
    When using the PWM input in open-loop mode, the DRV2604L device employs a fixed divider that observes the
    PWM signal and commutates the output drive signal at the PWM frequency divided by 128. To accomplish LRA
    drive, the host should drive the PWM frequency at 128 times the desired operating frequency.
*/
void play_pwm_frequency(rumble_data_s *data) 
{
    static bool hi_disabled = false;
    static bool lo_disabled = false;
    rumble_data_s playing_data = {0};

    if(!data->amplitude_high)
    {
        pwm_set_enabled(slice_num_hi, false);
        hi_disabled = true;
    }
    else hi_disabled = false;
        
    if(!data->amplitude_low)
    {
        pwm_set_enabled(slice_num_lo, false);
        lo_disabled = true;
    }
    else lo_disabled = false;
        

    data->frequency_high     = (data->frequency_high > 1300) ? 1300 : data->frequency_high;
    data->frequency_high     = (data->frequency_high < 40) ? 40 : data->frequency_high;

    data->frequency_low     = (data->frequency_low > 1300) ? 1300 : data->frequency_low;
    data->frequency_low     = (data->frequency_low < 40) ? 40 : data->frequency_low;

    //pwm_set_enabled(slice_num, false);
    //frequency *= 128; // Neets to multiply by 128 to get appropriate output signal

    // Calculate wrap value
    float target_wrap_hi = (float) PWM_CLOCK_BASE / data->frequency_high;
    float target_wrap_lo = (float) PWM_CLOCK_BASE / data->frequency_low;

    pwm_set_wrap(slice_num_hi, (uint16_t) target_wrap_hi);
    playing_data.frequency_high = data->frequency_high;
    pwm_set_wrap(slice_num_lo, (uint16_t) target_wrap_lo);
    playing_data.frequency_low = data->frequency_low;

    const float real_amp_range = 0.5f;
    const float min_amp = 0.05f;
	
    float min_amp_hi = 0;
    if(data->amplitude_high>0)
    {
        // Scale by scaler
        data->amplitude_high *= _rumble_scaler;

        min_amp_hi = data->amplitude_high * real_amp_range;
        min_amp_hi += min_amp;
    }

    float min_amp_lo = 0;
    if(data->amplitude_low>0)
    {
        // Scale by scaler
        data->amplitude_low *= _rumble_scaler;

        min_amp_lo = data->amplitude_low * real_amp_range;
        min_amp_lo += min_amp;
    }
 
    float amp_val_base_hi = target_wrap_hi * min_amp_hi;
    float amp_val_base_lo = target_wrap_lo * min_amp_lo;

    pwm_set_chan_level(slice_num_hi, LRA_HI_PWM_CHAN, (uint16_t) amp_val_base_hi); 
    playing_data.amplitude_high = amp_val_base_hi;
    pwm_set_chan_level(slice_num_lo, LRA_LOW_PWM_CHAN, (uint16_t) amp_val_base_lo); 
    playing_data.amplitude_low = amp_val_base_lo;
	
    if(!hi_disabled)
	pwm_set_enabled(slice_num_hi, true); // let's go!

    if(!lo_disabled)
    pwm_set_enabled(slice_num_lo, true); // let's go!
}

void cb_hoja_rumble_set(rumble_data_s *data)
{
    if(!_rumble_scaler) return;

    play_pwm_frequency(data);
}

void cb_hoja_rumble_test()
{
    rumble_data_s tmp = {.frequency_high = 320, .frequency_low = 160, .amplitude_high=1, .amplitude_low = 1};

    cb_hoja_rumble_set(&tmp);

    for(int i = 0; i < 62; i++)
    {   
        watchdog_update();
        sleep_ms(8);
    }

    tmp.amplitude_high = 0;
    tmp.amplitude_low = 0;
    
    cb_hoja_rumble_set(&tmp);
}


bool played = false;
void test_sequence()
{
    static rumble_data_s s = {0};
    if(played) return;
    played = true;
    for(int i = 0; i < 27; i+=1)
    {
        s.amplitude_low = 0.9f;
        s.frequency_low = song[i];
        play_pwm_frequency(&s);
        watchdog_update();
        sleep_ms(150);
    }
    sleep_ms(150);
    s.amplitude_low = 0;
    s.amplitude_high = 0;;
    play_pwm_frequency(&s);
    played = false;
}

bool lra_init = false;
// Obtain and dump calibration values for auto-init LRA
void cb_hoja_rumble_init()
{
    if(!lra_init)
    {
        sleep_ms(100);
        lra_init = true;
        
        // Set PWM function
        gpio_init(GPIO_LRA_IN_LO);
        gpio_set_function(GPIO_LRA_IN_LO, GPIO_FUNC_PWM);
        
        gpio_init(GPIO_LRA_IN_HI);
        gpio_set_function(GPIO_LRA_IN_HI, GPIO_FUNC_PWM);

        // Find out which PWM slice is connected to our GPIO pin
        slice_num_lo = pwm_gpio_to_slice_num(GPIO_LRA_IN_LO);
        slice_num_hi = pwm_gpio_to_slice_num(GPIO_LRA_IN_HI);

        // We want a 2Mhz clock driving the PWM
        // this means 1000 ticks = 2khz, 2000 ticks = 1khz
        float divider = 125000000.0f / PWM_CLOCK_BASE;

        // Set the PWM clock divider to run at 125MHz
        pwm_set_clkdiv(slice_num_lo, divider);
        pwm_set_clkdiv(slice_num_hi, divider);
    }

    uint8_t intensity = 0;
    rumble_type_t type;
    hoja_get_rumble_settings(&intensity, &type);

    if(!intensity) _rumble_scaler = 0;
    else _rumble_scaler = (float) intensity / 100.0f;

}

bool app_rumble_hwtest()
{

    test_sequence();
    //rumble_get_calibration();
    return true;
}

void app_rumble_task(uint32_t timestamp)
{
    return;
}