#include "hoja_includes.h"
#include "interval.h"
#include "main.h"
#include "math.h"
#include "float.h"

#define SOC_CLOCK (float)125000000.0f

#define SAMPLE_RATE 12000
#define BUFFER_SIZE 255
#define SAMPLE_FRAME_PERIOD 30
#define PWM_WRAP BUFFER_SIZE
#define PWM_PIN GPIO_LRA_IN_LO

#define STARTING_LOW_FREQ 160.0f
#define STARTING_HIGH_FREQ 320.0f

// Example from https://github.com/moefh/pico-audio-demo/blob/main/audio.c

#define REPETITION_RATE 4

static uint32_t single_sample = 0;
static uint32_t *single_sample_ptr = &single_sample;
static int pwm_dma_chan, trigger_dma_chan, sample_dma_chan;

volatile hoja_rumble_msg_s msg_left = {0};
volatile hoja_rumble_msg_s msg_right = {0};

static uint8_t audio_buffers[2][BUFFER_SIZE];
static volatile int cur_audio_buffer;
static volatile int last_audio_buffer;

#define M_PI 3.14159265358979323846

// Which sample we're processing
uint8_t samples_idx;
// How many sample frames we have to go through
uint8_t samples_count;
// How many samples we've generated
uint8_t sample_counter;

typedef struct
{
    float phase_step;
    float phase;
    float f;
    float f_step;
    float f_target;
    float a;
    float a_step;
    float a_target;
} haptic_s;

haptic_s hi_state = {0};
haptic_s lo_state = {0};

float clamp_rumble(float amplitude)
{
    const float min = 0.2f;
    const float max = 1.0f;
    const float range = max - min;

    float retval = 0;
    if (amplitude > 0)
    {
        retval = range * amplitude;
        retval += min;
        if (retval > max)
            retval = max;
        return retval;
    }
    return 0;
}

void generate_sine_wave(uint8_t *buffer)
{
    // This means we need to reset our state and
    // calculate our next step
    if (msg_left.unread)
    {
        msg_left.unread = false;
        samples_count = msg_left.sample_count;
        samples_idx = 0;
        sample_counter = 0;

        // Adjust hi frequency
        {
            float hif = msg_left.samples[0].high_frequency;
            hi_state.f_target = hif;
            hi_state.f_step = (hif - hi_state.f) / SAMPLE_FRAME_PERIOD;

            float hia = msg_left.samples[0].high_amplitude;
            hi_state.a_target = hia;
            hi_state.a_step = (hia - hi_state.a) / SAMPLE_FRAME_PERIOD;
        }

        // Adjust lo frequency
        {
            float lof = msg_left.samples[0].low_frequency;
            lo_state.f_target = lof;
            lo_state.f_step = (lof - lo_state.f) / SAMPLE_FRAME_PERIOD;

            float loa = msg_left.samples[0].low_amplitude;
            lo_state.a_target = loa;
            lo_state.a_step = (loa - lo_state.a) / SAMPLE_FRAME_PERIOD;
        }
    }

    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        hi_state.phase_step = (2 * M_PI * hi_state.f) / SAMPLE_RATE;
        lo_state.phase_step = (2 * M_PI * lo_state.f) / SAMPLE_RATE;

        float sample_high   = sinf(hi_state.phase);
        float sample_low    = sinf(lo_state.phase);

        // Scale samples by amplitude
        sample_high *= clamp_rumble(hi_state.a);
        sample_low  *= clamp_rumble(lo_state.a);

        // Combine samples
        float sample = ((sample_low * 0.6f) + (sample_high * 0.4f));

        // Ensure the sample is within [-1, 1] range
        if (sample > 1.0f)
            sample = 1.0f;
        if (sample < -1.0f)
            sample = -1.0f;

        buffer[i] = (uint8_t)((sample + 1.0f) * (127.5));

        // Update phases and steps
        {
            hi_state.phase += hi_state.phase_step;
            lo_state.phase += lo_state.phase_step;

            // Keep phases within [0, 2π) range
            hi_state.phase = fmodf(hi_state.phase, 2 * M_PI);
            lo_state.phase = fmodf(lo_state.phase, 2 * M_PI);

            if (sample_counter <= SAMPLE_FRAME_PERIOD)
            {
                hi_state.f += hi_state.f_step;
                hi_state.a += hi_state.a_step;
                lo_state.f += lo_state.f_step;
                lo_state.a += lo_state.a_step;
            }
            else
            {
                // Set values to target
                hi_state.f = hi_state.f_target;
                hi_state.a = hi_state.a_target;
                lo_state.f = lo_state.f_target;
                lo_state.a = lo_state.a_target;

                // increment sample_idx and prevent overflow
                if(samples_idx<100)
                    samples_idx++;
                
                if(samples_idx < samples_count)
                {
                    // reset sample_counter 
                    sample_counter = 0;

                    // Adjust hi frequency
                    float hif = msg_left.samples[samples_idx].high_frequency;
                    hi_state.f_target = hif;
                    hi_state.f_step = (hif - hi_state.f) / SAMPLE_FRAME_PERIOD;

                    float hia = msg_left.samples[samples_idx].high_amplitude;
                    hi_state.a_target = hia;
                    hi_state.a_step = (hia - hi_state.a) / SAMPLE_FRAME_PERIOD;

                    // Adjust lo frequency
                    float lof = msg_left.samples[samples_idx].low_frequency;
                    lo_state.f_target = lof;
                    lo_state.f_step = (lof - lo_state.f) / SAMPLE_FRAME_PERIOD;

                    float loa = msg_left.samples[samples_idx].low_amplitude;
                    lo_state.a_target = loa;
                    lo_state.a_step = (loa - lo_state.a) / SAMPLE_FRAME_PERIOD;
                }
            }

            // Increment sample_counter/prevent overflow
            if(sample_counter < 100)
                sample_counter++;
        }
    }
}

static void __isr __time_critical_func(dma_handler)()
{
    cur_audio_buffer = 1 - cur_audio_buffer;
    dma_hw->ch[sample_dma_chan].al1_read_addr = (intptr_t)&audio_buffers[cur_audio_buffer][0];
    dma_hw->ch[trigger_dma_chan].al3_read_addr_trig = (intptr_t)&single_sample_ptr;

    // Fill now-unused buffer
    uint8_t unused_buffer = 1 - cur_audio_buffer;
    generate_sine_wave(audio_buffers[unused_buffer]);

    dma_hw->ints1 = 1u << trigger_dma_chan;
}

void audio_init(int audio_pin, int sample_freq)
{
    gpio_set_function(audio_pin, GPIO_FUNC_PWM);

    int audio_pin_slice = pwm_gpio_to_slice_num(audio_pin);
    int audio_pin_chan = pwm_gpio_to_channel(audio_pin);

    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    float clock_div = ((float)f_clk_sys * 1000.0f) / 254.0f / (float)sample_freq / (float)REPETITION_RATE;

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_config_set_wrap(&config, 254);
    pwm_init(audio_pin_slice, &config, true);

    pwm_dma_chan = dma_claim_unused_channel(true);
    trigger_dma_chan = dma_claim_unused_channel(true);
    sample_dma_chan = dma_claim_unused_channel(true);

    // setup PWM DMA channel
    dma_channel_config pwm_dma_chan_config = dma_channel_get_default_config(pwm_dma_chan);
    channel_config_set_transfer_data_size(&pwm_dma_chan_config, DMA_SIZE_32);        // transfer 32 bits at a time
    channel_config_set_read_increment(&pwm_dma_chan_config, false);                  // always read from the same address
    channel_config_set_write_increment(&pwm_dma_chan_config, false);                 // always write to the same address
    channel_config_set_chain_to(&pwm_dma_chan_config, sample_dma_chan);              // trigger sample DMA channel when done
    channel_config_set_dreq(&pwm_dma_chan_config, DREQ_PWM_WRAP0 + audio_pin_slice); // transfer on PWM cycle end
    dma_channel_configure(pwm_dma_chan,
                          &pwm_dma_chan_config,
                          &pwm_hw->slice[audio_pin_slice].cc, // write to PWM slice CC register
                          &single_sample,                     // read from single_sample
                          REPETITION_RATE,                    // transfer once per desired sample repetition
                          false                               // don't start yet
    );

    // setup trigger DMA channel
    dma_channel_config trigger_dma_chan_config = dma_channel_get_default_config(trigger_dma_chan);
    channel_config_set_transfer_data_size(&trigger_dma_chan_config, DMA_SIZE_32);        // transfer 32-bits at a time
    channel_config_set_read_increment(&trigger_dma_chan_config, false);                  // always read from the same address
    channel_config_set_write_increment(&trigger_dma_chan_config, false);                 // always write to the same address
    channel_config_set_dreq(&trigger_dma_chan_config, DREQ_PWM_WRAP0 + audio_pin_slice); // transfer on PWM cycle end
    dma_channel_configure(trigger_dma_chan,
                          &trigger_dma_chan_config,
                          &dma_hw->ch[pwm_dma_chan].al3_read_addr_trig, // write to PWM DMA channel read address trigger
                          &single_sample_ptr,                           // read from location containing the address of single_sample
                          REPETITION_RATE * BUFFER_SIZE,                // trigger once per audio sample per repetition rate
                          false                                         // don't start yet
    );
    dma_channel_set_irq1_enabled(trigger_dma_chan, true); // fire interrupt when trigger DMA channel is done
    irq_set_exclusive_handler(DMA_IRQ_1, dma_handler);
    irq_set_enabled(DMA_IRQ_1, true);

    // setup sample DMA channel
    dma_channel_config sample_dma_chan_config = dma_channel_get_default_config(sample_dma_chan);
    channel_config_set_transfer_data_size(&sample_dma_chan_config, DMA_SIZE_8); // transfer 8-bits at a time
    channel_config_set_read_increment(&sample_dma_chan_config, true);           // increment read address to go through audio buffer
    channel_config_set_write_increment(&sample_dma_chan_config, false);         // always write to the same address
    dma_channel_configure(sample_dma_chan,
                          &sample_dma_chan_config,
                          (char *)&single_sample + 2 * audio_pin_chan, // write to single_sample
                          &audio_buffers[0][0],                        // read from audio buffer
                          1,                                           // only do one transfer (once per PWM DMA completion due to chaining)
                          false                                        // don't start yet
    );

    // clear audio buffers
    memset(audio_buffers[0], 128, BUFFER_SIZE);
    memset(audio_buffers[1], 128, BUFFER_SIZE);

    // kick things off with the trigger DMA channel
    dma_channel_start(trigger_dma_chan);
}

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
    B3, E4, A4, E4, D5, E4, Db5, Ab4, Eb4, Ab4, Fs4, Db4, Ab3, Db4, D4, G4, C5, F5, Bb5, F5, C5, A5, E5, B4, Ab5, 0, 0, E4};

float _rumble_scaler = 0;

void cb_hoja_rumble_set(hoja_rumble_msg_s *left, hoja_rumble_msg_s *right)
{
    memcpy(&msg_left, left, sizeof(hoja_rumble_msg_s));
    msg_left.unread = true;
}

void cb_hoja_rumble_test()
{
    rumble_data_s tmp = {.frequency_high = 320, .frequency_low = 160, .amplitude_high = 0, .amplitude_low = 1};

    // cb_hoja_rumble_set(&tmp);

    for (int i = 0; i < 62; i++)
    {
        watchdog_update();
        sleep_ms(8);
    }

    tmp.amplitude_high = 0;
    tmp.amplitude_low = 0;

    // cb_hoja_rumble_set(&tmp);
}

bool played = false;
void test_sequence()
{
    static rumble_data_s s = {0};
    if (played)
        return;
    played = true;
    for (int i = 0; i < 27; i += 1)
    {
        s.amplitude_low = 0.9f;
        s.frequency_low = song[i];
        // play_pwm_frequency(&s);
        watchdog_update();
        sleep_ms(150);
    }
    sleep_ms(150);
    s.amplitude_low = 0;
    s.amplitude_high = 0;
    ;
    // play_pwm_frequency(&s);
    played = false;
}

bool lra_init = false;
// Obtain and dump calibration values for auto-init LRA
void cb_hoja_rumble_init()
{
    if (!lra_init)
    {
        sleep_ms(100);
        lra_init = true;
    }

    uint8_t intensity = 0;
    rumble_type_t type;
    hoja_get_rumble_settings(&intensity, &type);

    if (!intensity)
        _rumble_scaler = 0;
    else
        _rumble_scaler = (float)intensity / 100.0f;

    // Generate initial buffers
    generate_sine_wave(audio_buffers[0]);
    generate_sine_wave(audio_buffers[1]);

    audio_init(GPIO_LRA_IN_LO, SAMPLE_RATE);
}

bool app_rumble_hwtest()
{

    test_sequence();
    // rumble_get_calibration();
    return true;
}

void app_rumble_task(uint32_t timestamp)
{
    return;
}