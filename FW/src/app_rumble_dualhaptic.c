#include "hoja_includes.h"
#include "interval.h"
#include "main.h"
#include "math.h"
#include "float.h"

void app_rumble_task(uint32_t timestamp);
bool app_rumble_hwtest();

#define SOC_CLOCK (float)HOJA_SYS_CLK_HZ

#define SAMPLE_RATE 12000
#define BUFFER_SIZE 255
#define SAMPLE_TRANSITION 30
#define PWM_WRAP BUFFER_SIZE

// LRA Audio Output
#define LRA_L_PWM_CHAN      LRA_LOW_PWM_CHAN
#define LRA_R_PWM_CHAN      LRA_HI_PWM_CHAN
#define PWM_PIN_L GPIO_LRA_IN_LO //GPIO_LRA_IN_LO
#define PWM_PIN_R GPIO_LRA_IN_HI //GPIO_LRA_IN_HI



#define STARTING_LOW_FREQ 160.0f
#define STARTING_HIGH_FREQ 320.0f

// Example from https://github.com/moefh/pico-audio-demo/blob/main/audio.c

#define REPETITION_RATE 4

uint dma_cc_l, dma_cc_r, dma_trigger_l, dma_trigger_r, dma_sample;
uint pwm_slice_l, pwm_slice_r;
uint32_t dma_trigger_start_mask = 0;

uint32_t single_sample;
uint32_t *single_sample_ptr = &single_sample;

//static volatile int cur_audio_buffer;

#define M_PI 3.14159265358979323846
#define TWO_PI (M_PI*2)

// The current audio buffer in use
static volatile uint audio_buffer_idx = 0;
static uint32_t audio_buffers[2][BUFFER_SIZE] = {0};

// Bool to indicate we need to fill our next sine wave
static volatile bool ready_next_sine = false;

float _rumble_scaler = 1;

typedef struct
{
    // How much we increase the phase each time
    float phase_step;
    // How much to increase our phase step each time
    float phase_accumulator;
    // Our current phase
    float phase;
    float f;
    float f_step;
    float f_target;
    float a;
    float a_step;
    float a_target;
} haptic_s;

typedef struct
{
    haptic_s lo_state;
    haptic_s hi_state;
    // Which sample we're processing
    uint8_t samples_idx;
    // How many sample frames we have to go through
    uint8_t samples_count;
    // How many samples we've generated
    uint8_t sample_counter;
} haptic_both_s;


haptic_both_s haptics_left = {0};
haptic_both_s haptics_right = {0};

float clamp_rumble_lo(float amplitude)
{
    const float min = 0.05f;
    const float max = 1.0f;

    const float upper_expander = 0.75f; // We want to treat 0 to 0.75 as 0 to 1

    if(!amplitude) return 0;
    if(!_rumble_scaler) return 0;

    float expanded_amp = amplitude/upper_expander;
    expanded_amp = (expanded_amp>1) ? 1 : expanded_amp;

    float range = max - min;
    range *= _rumble_scaler;

    float retval = 0;

    retval = range * expanded_amp;
    retval += min;
    if (retval > max)
        retval = max;
    return retval;

}

float clamp_rumble_hi(float amplitude)
{
    const float min = 0.05f;
    const float max = 1.0f;

    const float upper_expander = 0.75f; // We want to treat 0 to 0.75 as 0 to 1

    if(!amplitude) return 0;
    if(!_rumble_scaler) return 0;

    float expanded_amp = amplitude/upper_expander;
    expanded_amp = (expanded_amp>1) ? 1 : expanded_amp;

    float range = max - min;
    range *= _rumble_scaler;

    float retval = 0;

    retval = range * expanded_amp;
    retval += min;
    if (retval > max)
        retval = max;
    return retval;

}

#define SIN_TABLE_SIZE 4096
int16_t sin_table[SIN_TABLE_SIZE] = {0};
#define SIN_RANGE_MAX 128

void sine_table_init()
{
    float inc = TWO_PI / SIN_TABLE_SIZE;
    float fi = 0;

    for (int i = 0; i < SIN_TABLE_SIZE; i++)
    {
        float sample = sinf(fi);

        sin_table[i] = (int16_t)(sample * SIN_RANGE_MAX);

        fi += inc;
        fi = fmodf(fi, TWO_PI);
    }
}

// This function is designed to be broken
// up into many steps to avoid causing slowdown
// or starve other tasks
// Returns true when the buffer is filled
void generate_sine_wave(uint32_t *buffer, haptic_both_s *state)
{
    uint16_t transition_idx = 0;
    uint8_t sample_idx = 0;
    amfm_s cur[3] = {0};
    int8_t samples = haptics_get(true, cur, false, NULL);

    for (uint16_t i = 0; i < BUFFER_SIZE; i++)
    {
        // Check if we are at the start of a new block of waveform
        if (!transition_idx)
        {
            if ((samples > 0) && (sample_idx < samples))
            {
                // Set high frequency
                float hif = cur[sample_idx].f_hi;
                float hia = cur[sample_idx].a_hi;

                state->hi_state.f_target = hif;
                state->hi_state.f_step = (hif - state->hi_state.f) / SAMPLE_TRANSITION;

                state->hi_state.phase_step = (SIN_TABLE_SIZE * state->hi_state.f) / SAMPLE_RATE;
                float hphase_step_end = (SIN_TABLE_SIZE * state->hi_state.f_target) / SAMPLE_RATE;
                state->hi_state.phase_accumulator = (hphase_step_end - state->hi_state.phase_step) / SAMPLE_TRANSITION;

                state->hi_state.a_target = hia;
                state->hi_state.a_step = (hia - state->hi_state.a) / SAMPLE_TRANSITION;

                float lof = cur[sample_idx].f_lo;
                float loa = cur[sample_idx].a_lo;

                // Set low frequency
                state->lo_state.f_target = lof;
                state->lo_state.f_step = (lof - state->lo_state.f) / SAMPLE_TRANSITION;

                state->lo_state.phase_step = (SIN_TABLE_SIZE * state->lo_state.f) / SAMPLE_RATE;
                float lphase_step_end = (SIN_TABLE_SIZE * state->lo_state.f_target) / SAMPLE_RATE;
                state->lo_state.phase_accumulator = (lphase_step_end - state->lo_state.phase_step) / SAMPLE_TRANSITION;

                state->lo_state.a_target = loa;
                state->lo_state.a_step = (loa - state->lo_state.a) / SAMPLE_TRANSITION;

                sample_idx++;
            }
            else
            {
                // reset all steps so we just
                // continue generating
                state->hi_state.f_step = 0;
                state->hi_state.a_step = 0;
                state->lo_state.f_step = 0;
                state->lo_state.a_step = 0;
                state->hi_state.phase_accumulator = 0;
                state->lo_state.phase_accumulator = 0;
            }
        }

        if (state->hi_state.phase_accumulator > 0)
            state->hi_state.phase_step += state->hi_state.phase_accumulator;

        if (state->lo_state.phase_accumulator > 0)
            state->lo_state.phase_step += state->lo_state.phase_accumulator;

        float sample_high = sin_table[(uint16_t)state->hi_state.phase];
        float sample_low = sin_table[(uint16_t)state->lo_state.phase];

        float hi_a = clamp_rumble_hi(state->hi_state.a);
        float lo_a = clamp_rumble_lo(state->lo_state.a);
        float comb_a = hi_a + lo_a;

        if ((comb_a) > 1.0f)
        {
            float new_ratio = 1.0f / (comb_a);
            hi_a *= new_ratio;
            lo_a *= new_ratio;
        }

        sample_high *= hi_a;
        sample_low *= lo_a;

        // Combine samples
        float sample = sample_low + sample_high;
        //sample*=0.45f;

        sample = (sample > 255) ? 255 : (sample < 0) ? 0
                                                     : sample;

        uint32_t outsample = ((uint32_t) sample << 16) | (uint8_t) sample;
        buffer[i] = outsample;

        // Update phases and steps
        state->hi_state.phase += state->hi_state.phase_step;
        state->lo_state.phase += state->lo_state.phase_step;

        // Keep phases within range
        state->hi_state.phase = fmodf(state->hi_state.phase, SIN_TABLE_SIZE);
        state->lo_state.phase = fmodf(state->lo_state.phase, SIN_TABLE_SIZE);

        if (transition_idx < SAMPLE_TRANSITION)
        {
            state->hi_state.f += state->hi_state.f_step;
            state->hi_state.a += state->hi_state.a_step;

            state->lo_state.f += state->lo_state.f_step;
            state->lo_state.a += state->lo_state.a_step;

            transition_idx++;
        }
        else
        {
            // Set values to target
            state->hi_state.f = state->hi_state.f_target;
            state->hi_state.a = state->hi_state.a_target;
            state->lo_state.f = state->lo_state.f_target;
            state->lo_state.a = state->lo_state.a_target;
            state->hi_state.phase_accumulator = 0;
            state->lo_state.phase_accumulator = 0;

            // Reset sample index
            transition_idx = 0;
        }
    }
}

static void __isr __time_critical_func(dma_handler)()
{
    audio_buffer_idx = 1-audio_buffer_idx;
    dma_hw->ch[dma_sample].al1_read_addr    = (intptr_t) &audio_buffers[audio_buffer_idx][0];
    dma_hw->ch[dma_trigger_l].al1_read_addr = (intptr_t) &single_sample_ptr;
    dma_hw->ch[dma_trigger_r].al1_read_addr = (intptr_t) &single_sample_ptr;
    
    dma_start_channel_mask(dma_trigger_start_mask);
    ready_next_sine = true;
    dma_hw->ints1 = 1u << dma_trigger_l;
}

void audio_init(int audio_pin_l, int audio_pin_r, int sample_freq)
{
    sine_table_init();

    gpio_set_function(PWM_PIN_L, GPIO_FUNC_PWM);
    gpio_set_function(PWM_PIN_R, GPIO_FUNC_PWM);

    pwm_slice_l = pwm_gpio_to_slice_num(PWM_PIN_L);
    pwm_slice_r = pwm_gpio_to_slice_num(PWM_PIN_R);

    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    float clock_div = ((float)f_clk_sys * 1000.0f) / 254.0f / (float)sample_freq / (float)REPETITION_RATE;

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_config_set_wrap(&config, 254);

    pwm_init(pwm_slice_l, &config, false);
    pwm_init(pwm_slice_r, &config, false);

    uint32_t pwm_enable_mask = (1U<<pwm_slice_l) | (1U<<pwm_slice_r);
    pwm_set_mask_enabled(pwm_enable_mask);

    dma_cc_l = dma_claim_unused_channel(true);
    dma_cc_r = dma_claim_unused_channel(true);
    dma_trigger_l = dma_claim_unused_channel(true);
    dma_trigger_r = dma_claim_unused_channel(true);
    dma_sample = dma_claim_unused_channel(true);

    // setup PWM DMA channel LEFT
    dma_channel_config dma_cc_l_cfg = dma_channel_get_default_config(dma_cc_l);

    channel_config_set_transfer_data_size(&dma_cc_l_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_cc_l_cfg, false);
    channel_config_set_write_increment(&dma_cc_l_cfg, false);
    channel_config_set_chain_to(&dma_cc_l_cfg, dma_sample);               // Chain to increment DMA sample
    channel_config_set_dreq(&dma_cc_l_cfg, DREQ_PWM_WRAP0 + pwm_slice_l); // transfer on PWM cycle end

    dma_channel_configure(dma_cc_l,
                          &dma_cc_l_cfg,
                          &pwm_hw->slice[pwm_slice_l].cc, // write to PWM slice CC register (Compare)
                          &single_sample,
                          REPETITION_RATE, // Write the sample to the CC 4 times
                          false);

    // setup PWM DMA channel RIGHT
    dma_channel_config dma_cc_r_cfg = dma_channel_get_default_config(dma_cc_r);

    channel_config_set_transfer_data_size(&dma_cc_r_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_cc_r_cfg, false);
    channel_config_set_write_increment(&dma_cc_r_cfg, false);
    // We do not chain the right channel to anything
    channel_config_set_dreq(&dma_cc_r_cfg, DREQ_PWM_WRAP0 + pwm_slice_r); // transfer on PWM cycle end

    dma_channel_configure(dma_cc_r,
                          &dma_cc_r_cfg,
                          &pwm_hw->slice[pwm_slice_r].cc, // write to PWM slice CC register (Compare)
                          &single_sample,
                          REPETITION_RATE, // Write the sample to the CC 4 times
                          false);

    // setup trigger DMA channel LEFT
    dma_channel_config dma_trigger_l_cfg = dma_channel_get_default_config(dma_trigger_l);
    channel_config_set_transfer_data_size(&dma_trigger_l_cfg, DMA_SIZE_32);    // transfer 32-bits at a time
    channel_config_set_read_increment(&dma_trigger_l_cfg, false);              // always read from the same address
    channel_config_set_write_increment(&dma_trigger_l_cfg, false);             // always write to the same address
    channel_config_set_dreq(&dma_trigger_l_cfg, DREQ_PWM_WRAP0 + pwm_slice_l); // transfer on PWM cycle end

    dma_channel_configure(dma_trigger_l,
                          &dma_trigger_l_cfg,
                          &dma_hw->ch[dma_cc_l].al3_read_addr_trig, // write to PWM DMA channel read address trigger
                          &single_sample_ptr,                       // read from location containing the address of single_sample
                          REPETITION_RATE * BUFFER_SIZE,            // trigger once per audio sample per repetition rate
                          false);

    dma_channel_set_irq1_enabled(dma_trigger_l, true); // fire interrupt when trigger DMA channel is done
    // We do not set up our ISR for dma_trigger_l since they are in sync
    irq_set_exclusive_handler(DMA_IRQ_1, dma_handler);
    irq_set_enabled(DMA_IRQ_1, true);

    // setup trigger DMA channel RIGHT
    dma_channel_config dma_trigger_r_cfg = dma_channel_get_default_config(dma_trigger_r);
    channel_config_set_transfer_data_size(&dma_trigger_r_cfg, DMA_SIZE_32);    // transfer 32-bits at a time
    channel_config_set_read_increment(&dma_trigger_r_cfg, false);              // always read from the same address
    channel_config_set_write_increment(&dma_trigger_r_cfg, false);             // always write to the same address
    channel_config_set_dreq(&dma_trigger_r_cfg, DREQ_PWM_WRAP0 + pwm_slice_r); // transfer on PWM cycle end

    dma_channel_configure(dma_trigger_r,
                          &dma_trigger_r_cfg,
                          &dma_hw->ch[dma_cc_r].al3_read_addr_trig, // write to PWM DMA channel read address trigger
                          &single_sample_ptr,                       // read from location containing the address of single_sample
                          REPETITION_RATE * BUFFER_SIZE,            // trigger once per audio sample per repetition rate
                          false);

    // setup sample DMA channel
    dma_channel_config dma_sample_cfg = dma_channel_get_default_config(dma_sample);
    channel_config_set_transfer_data_size(&dma_sample_cfg, DMA_SIZE_32); // transfer 8-bits at a time
    channel_config_set_read_increment(&dma_sample_cfg, true);            // increment read address to go through audio buffer
    channel_config_set_write_increment(&dma_sample_cfg, false);          // always write to the same address
    dma_channel_configure(dma_sample,
                          &dma_sample_cfg,
                          &single_sample,       // write to single_sample. This contains the data for both CH A and CH B
                          &audio_buffers[0][0], // read from audio buffer
                          1,                    // only do one transfer (once per PWM DMA completion due to chaining)
                          false                 // don't start yet
    );

    // clear audio buffers
    memset(audio_buffers[0], 0, BUFFER_SIZE);
    memset(audio_buffers[1], 0, BUFFER_SIZE);

    // kick things off with the trigger DMA channels
    dma_trigger_start_mask = (1U<<dma_trigger_r) | (1U<<dma_trigger_l);
    dma_start_channel_mask(dma_trigger_start_mask);
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


void cb_hoja_rumble_test()
{
    haptics_set_all(0, 0, HOJA_HAPTIC_BASE_LFREQ, 0);
    sleep_ms(8);
    app_rumble_task(0);
    haptics_set_all(0, 0, HOJA_HAPTIC_BASE_LFREQ, 1);

    for (int i = 0; i < 62; i++)
    {
        app_rumble_task(0);
        watchdog_update();
        sleep_ms(8);
    }

    haptics_set_all(0, 0, HOJA_HAPTIC_BASE_LFREQ, 0);
    haptics_set_all(0, 0, HOJA_HAPTIC_BASE_LFREQ, 0);
}

bool played = false;
void test_sequence()
{

    return;
    if(played) return;
    played = true;
    for(int i = 0; i < 27; i+=1)
    {
        //msg.samples[0].low_amplitude = 0.9f;
        //msg.samples[0].low_frequency = song[i];

        watchdog_update();
        sleep_ms(150);
    }
    sleep_ms(150);


    played = false;
}

bool lra_init = false;
void cb_hoja_rumble_init()
{
    if (!lra_init)
    {
        lra_init = true;
        audio_init(PWM_PIN_L, PWM_PIN_R, SAMPLE_RATE);
    }

    uint8_t intensity = 0;
    rumble_type_t type;
    hoja_get_rumble_settings(&intensity, &type);

    if (!intensity)
        _rumble_scaler = 0;
    else
        _rumble_scaler = (float)intensity / 100.0f;
}

bool app_rumble_hwtest()
{

    test_sequence();
    // rumble_get_calibration();
    return true;
}

void app_rumble_task(uint32_t timestamp)
{
    if (ready_next_sine)
    {
        ready_next_sine = false;
        uint8_t available_buffer = 1 - audio_buffer_idx;
        generate_sine_wave(audio_buffers[available_buffer], &haptics_left);
    }
}