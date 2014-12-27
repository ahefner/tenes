#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <SDL.h>
#include <signal.h>
#include <math.h>
#include "sys.h"
#include "nes.h"
#include "sound.h"


/* TODO: Move audio state out of random global variables and into the NES struct.

   FIXME: Buffer underruns. I've made them inaudible with the emergency catchup
   sample generation, but this isn't ideal and breaks determinisms with certain games.

   FIXME: Audio frequencies are off by a bit due to rounding of CLOCK/48000.

   FIXME: Interrupt acknowledgement. Right now we fire interrupts once, rather
          than emulating the real behavior of the interrupt signals, so it's
          possible to lose interrupts if e.g. the frameseq and DMC IRQs occur
          simultaneously, or (more likely) maskable interrupts are disabled.
*/

#define AUDIO_BUFFER_SIZE 16384
#define BUFFER_PTR_MASK   (AUDIO_BUFFER_SIZE-1)

/* Audio is output to a mirrored circular buffer with low/high water mark */
volatile int buffer_low = 0;
volatile int buffer_high = 0;
static Sint16 audio_buffer[AUDIO_BUFFER_SIZE*2];

int sound_initialized = 0;
volatile int sound_enabled = 1;

static void buffer_init (void);
static void audio_callback (void *udata, Sint16 * stream, int len);
static void snd_fillbuffer (Sint16 * buf, unsigned index, unsigned length);
void snd_render_samples (int emergency_mode, int samples);

static SDL_AudioSpec desired, obtained;

int snd_init (void)
{
    producer_mutex = SDL_CreateMutex();

    if (sound_globalenabled) {

        buffer_init();

        desired.freq = 48000;
        desired.format = AUDIO_S16 /* | AUDIO_MONO */ ;
        desired.samples = 512; /* Desired buffer size */
        desired.callback = (void *)audio_callback;
        desired.channels = 1;
        desired.userdata = NULL;

        printf ("Opening audio device.\n");
        if (SDL_OpenAudio (&desired, &obtained)) {
            printf ("SDL_OpenAudio failed: %s", SDL_GetError ());
            sound_enabled = 0;
            return -1;
        } else {
            printf ("SDL audio initialized. Buffer size = %i. Sample rate = %i\n", obtained.samples, obtained.freq);
            if (obtained.samples > AUDIO_BUFFER_SIZE)
                printf("Obtained audio buffer size is too large! Sound will be garbled.\n");
            sound_initialized = 1;
            SDL_PauseAudio(0);

            return 0;
        }
    } else {
        sound_enabled = 0;
        return 0;
    }
}

void snd_shutdown (void)
{
    /* Setting sound_enabled breaks audio_callback out of its wait loop. */
    sound_enabled = 0;
    if (sound_initialized) SDL_CloseAudio();
    SDL_DestroyMutex(producer_mutex);

}

/* buffer_samples: Returns used space in the audio buffer */
static int buffer_samples (void)
{
    return buffer_high - buffer_low;
}

/* Thread-safe version for public use. Desirable for the NSF player,
 * to let it buffer ahead from the main thread. */
int snd_buffered_samples (void)
{
    SDL_mutexP(producer_mutex);
    int n = buffer_samples();
    SDL_mutexV(producer_mutex);
    return n;
}

/* buffer_space: Returns available space in the audio buffer */
static int buffer_space (void)
{
    assert(buffer_samples() <= AUDIO_BUFFER_SIZE);
    assert(buffer_samples() >= 0);
    return AUDIO_BUFFER_SIZE - buffer_samples();
}

static void buffer_init (void)
{
    memset((void *) audio_buffer, 0, AUDIO_BUFFER_SIZE*2);

    buffer_low = 0;
    /* Odd: */
    buffer_high = AUDIO_BUFFER_SIZE / 2;
}

const int desired_buffer_ahead = 500;
static int first_buffer = 1;
static int delta_log[16];
static int fill_log[16];
static int min_buffer;
static int max_buffer;
static long long time_log[16];
int delta_log_idx = 0;

/* This is not exactly accurate. */
int apu_interrupt_pending = 0;

static void signal_dmc_interrupt (void)
{
    apu_interrupt_pending = 1;
}

static void signal_frameseq_interrupt (void)
{
    apu_interrupt_pending = 1;
}

void service_interrupts (void)
{
    if (apu_interrupt_pending) Int6502(&nes.cpu, INT_IRQ);
    apu_interrupt_pending = 0;
}

static void audio_callback (void *udata, Sint16 *stream, int len)
{
    int req = len >> 1, consumed = req;
    int ideal_buffer_length = req + desired_buffer_ahead;

    int num = buffer_samples();
    static int last = 0;
    static long long last_time = 0;
    long long current_time = usectime(), delta_time = current_time - last_time;
    last_time = current_time;

    /* There appears to be some considerable delay between starting up
     * the audio system and the first invocation of the callback, such
     * that there is more audio buffered than you'd expect. Therefore,
     * snap to the end of the buffer, minus however much we'd prefer
     * to keep as a safety margin. */
    if (first_buffer) {
        // Wait for the main thread to produce enough audio.
        // It most likely already has.
        while (buffer_samples() < ideal_buffer_length) { /* Wait. */ }

        buffer_low = buffer_high - min(buffer_samples(), ideal_buffer_length);
        num = buffer_samples();
        first_buffer = 0;
        /* Init stats logs: */
        memset(delta_log, 0, sizeof(delta_log));
        memset(fill_log, 0, sizeof(fill_log));
        memset(time_log, 0, sizeof(time_log));
    } else {
        int i, avg_x = 0, avg_dx = 0;
        long long avg_time = 0;
        time_log[delta_log_idx] = delta_time;
        delta_log[delta_log_idx] = num - last;
        last = num;
        fill_log[delta_log_idx] = num;
        delta_log_idx = (delta_log_idx + 1) % 16;
        if (!delta_log_idx) {
            max_buffer = 0;
            min_buffer = AUDIO_BUFFER_SIZE;
        } else {
            min_buffer = min(min_buffer, num);
            max_buffer = max(max_buffer, num);
        }

        for (i=0; i<16; i++) { avg_dx += delta_log[i]; avg_x += fill_log[i]; avg_time += time_log[i]; }
        avg_x /= i;
        avg_dx /= i;
        avg_time /= (long long)i;
        if (0 && delta_log_idx == 15) {
            printf("Average drift: %4i, average fill: %4i (min %4i max %4i), avg time = %5lli (expecting %5f)\n", avg_dx, avg_x,
                   min_buffer, max_buffer, avg_time, 1000000.0 / (48000.0 / len * 2.0));

        }
    }

    //printf("delta=%4f ms   fill=%4i   req=%4i\n", delta_time / 1000.0, num, req);

    last = num;

    //if (num == AUDIO_BUFFER_SIZE) printf("Audio buffer full :(\n");

    if (req > num) {
        SDL_mutexP(producer_mutex);
        snd_render_samples(1, (req - num) + 128);
        SDL_mutexV(producer_mutex);
        memset(delta_log, 0, sizeof(delta_log));
        /* Don't pollute traces with audio messages */
/*
        if (!(trace_ppu_writes || nes.cpu.Trace))
            printf("Underrun! requested %i, %i available. Time since last callback: %i us\n", req, num, (int)delta_time);
*/
    }

    if (sound_enabled) memcpy(stream, audio_buffer + (buffer_low & BUFFER_PTR_MASK), len);

    buffer_low += consumed;
    //printf("consumed %i samples (%i/%i).\n", consumed, buffer_low, buffer_high);

}

/* the pAPU loads the length counter with some pretty strange things.. */
int translate_length (byte value)
{
    static int lengths[2][16] =
        {{0x0a,0x14,0x28,0x50,0xA0,0x3C,0x0E,0x1A,0x0C,0x18,0x30,0x60,0xC0,0x48,0x10,0x20},
         {0xfe,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1E}};

    return lengths[(value>>3)&1][value>>4];
}

/* PPU clock frequency - CPU runs at half of this */
#define CLOCK2 3579545
#define CLOCK  1789772

/** Sound State **/

#define SND (nes.snd)

void snd_reset (void)
{
    memset(&SND, 0, sizeof(SND));
    SND.frameseq_divider = 200;
    SND.frameseq_irq_disable = 1;
    SND.dmc_address = 0x8000;
}

const int out_counter_mask[3]={0x0F,0x0F,0x1F};

void frameseq_clock_divider (void);
void frameseq_clock_sequencer (void);
void length_counters_clock (void);
void sweep_clock (void);
void envelope_clock (void);
void linear_counter_clock (void);

/** The length counter divides the system clock down to 240 Hz and steps
    through a sequence which clocks the envelopes, linear counter,
    length counters, sweep unit, and 60 Hz interrupts.
**/

const int frameseq_divider_reset = 200; /* 48000 Hz / 240 Hz */

#define FRAMESEQ_CLOCK_ENV_LIN 1
#define FRAMESEQ_CLOCK_LEN_SWEEP 2
#define FRAMESEQ_CLOCK_IRQ 4

int frameseq_patterns[2][5] = { { 1, 3, 1, 7, 0}, { 3, 1, 3, 1, 0} };

/* Clock the frameseq divider. Should be called at 48000 Hz. */
void frameseq_clock_divider (void) {
    if (!SND.frameseq_divider) {
        frameseq_clock_sequencer();
    } else SND.frameseq_divider--;
}

/* Clock the sequencer. Called when divider counts down. */
void frameseq_clock_sequencer (void)
{
        int output;
        SND.frameseq_divider = frameseq_divider_reset;

        output = frameseq_patterns[SND.frameseq_mode_5][SND.frameseq_sequencer];
        if ((output & FRAMESEQ_CLOCK_IRQ) && !SND.frameseq_irq_disable) {
            nes.snd.regs[0x15] |= 0x40;
            signal_frameseq_interrupt();
        }

        if (output & FRAMESEQ_CLOCK_LEN_SWEEP) {
            length_counters_clock();
            sweep_clock();
        }

        if (output & FRAMESEQ_CLOCK_ENV_LIN) {
            envelope_clock();
            linear_counter_clock();
        }

        SND.frameseq_sequencer++;
        if ((SND.frameseq_sequencer == 5) ||
            ((SND.frameseq_sequencer == 4) && !SND.frameseq_mode_5)) SND.frameseq_sequencer = 0;
}

/** Length counters count down unless the hold bit is set. **/

static inline int tick (int value) { return value? value-1 : 0; }

void length_counters_clock (void)
{
    byte *r=nes.snd.regs;

    if ( !(r[0] & 0x20)) SND.lcounter[0] = tick(SND.lcounter[0]);
    if ( !(r[4] & 0x20)) SND.lcounter[1] = tick(SND.lcounter[1]);
    if ( !(r[8] & 0x80)) SND.lcounter[2] = tick(SND.lcounter[2]);
    if (!(r[12] & 0x20)) SND.lcounter[3] = tick(SND.lcounter[3]);
}

/** Sweep unit **/

void sweep_clock_channel (int channel)
{
    int reg = nes.snd.regs[channel*4+1],
        enabled = reg & 0x80,
        period = ((reg>>4) & 7),
        negate = reg & 8,
        shift = reg & 7;
    int wl = SND.wavelength[channel];

    assert(SND.sweep_divider[channel] >= 0);

    if (0 && !channel) {
        nes_printtime();
        printf("sweep clocked for channel %x. sweep reg=%02X. divider=%i\n", channel, reg, SND.sweep_divider[channel]);
    }

    /* Clock the divider */
    if (!SND.sweep_divider[channel]) {
        int shifted = wl >> shift;
        int sum = wl + (negate? (-shifted - (channel^1)) : shifted);


        if (0 && (channel == 0)) {
            printf("%u: ch0 sweep enabled: %i, period = %x, negate = %x, shift=%x   wl=%i shifted=%i  LC=%i\n",
                   nes.time, enabled?1:0, period, negate, shift, wl, shifted, SND.lcounter[0]);
        }


        if (enabled && (shift>0) && (wl >= 8) && (sum <= 0x7FF)) SND.wavelength[channel] = sum;
        if (SND.wavelength[channel] < 0) {
            SND.wavelength[channel] = 0;
            printf("%u: Sweep underflow on channel %i. (?)\n", nes.time, channel);
        }

        /* The sweep can silence the channel. Exactly how is not
           mentioned, so I elect to zero the length counter. We only
           need to do this on carry, for sweep up, because a sweep
           down will eventually drop the wavelength below 8, at which
           time the channel is muted at the output stage. */
        if (sum >= 0x800) SND.lcounter[channel] = 0;

        SND.sweep_divider[channel] = period;
    } else SND.sweep_divider[channel]--;

    /* Check for reset */
    if (SND.sweep_reset[channel]) {
        SND.sweep_reset[channel] = 0;
        SND.sweep_divider[channel] = period;

        if (0) printf(" sweep reset channel %x divider to %X\n", channel, period);
    }
}

void sweep_clock (void)
{
    sweep_clock_channel(0);
    sweep_clock_channel(1);
}

/** Envelope counter **/

static inline void envelope_compute_output_volume (int channel)
{
    int const_vol = (nes.snd.regs[channel<<2] & BIT(4));
    if (!const_vol) SND.volume[channel] = SND.envc[channel];
    else SND.volume[channel] = nes.snd.regs[channel<<2] & 0x0F;
}

static inline void envelope_counter_clock (int channel)
{
    if (SND.envc[channel]) SND.envc[channel]--;
    else if (nes.snd.regs[channel<<2] & 0x20) SND.envc[channel] = 15; /* Loop envelope */
    envelope_compute_output_volume(channel);
}

void envelope_clock (void)
{
    int i;

    for (i=0; i<4; i++) {
        if (SND.env_reset[i]) {     /* Reset divider? */
            SND.env_reset[i] = 0;
            SND.envc[i] = 15;
            SND.envc_divider[i] = (nes.snd.regs[i<<2] & 0x0F);
        } else {                /* Clock divider. */
            if (!SND.envc_divider[i]) {
                SND.envc_divider[i] = (nes.snd.regs[i<<2] & 0x0F);
                envelope_counter_clock(i);
            } else SND.envc_divider[i]--;
        }
    }
}

/** Linear Counter (triangle channel) **/

void linear_counter_clock (void)
{
    // printf("%u: linear_counter clocked. currently %i. halted? %s.regs = %02X %02X\n", nes.time, SND.linear_counter, SND.linear_counter_halt? "Yes":"No", nes.snd.regs[8], nes.snd.regs[11]);
    if (SND.linear_counter_halt) {
        SND.linear_counter = nes.snd.regs[8] & 0x7F;
        // printf("  %u: Linear counter took new value %02X.\n", nes.time, SND.linear_counter);
    } else if (SND.linear_counter) SND.linear_counter--;

    if (!(nes.snd.regs[8]&0x80)) SND.linear_counter_halt = 0;
}

int translate_noise_period (int n)
{
    const int periods[16] = { 0x004, 0x008, 0x010, 0x020, 0x040, 0x060,
                              0x080, 0x0A0, 0x0CA, 0x0FE, 0x17C, 0x1FC,
                              0x2FA, 0x3F8, 0x7F2, 0xFE4 };
    return periods[n];
}

/** Delta Modulation Channel **/

/* We don't have a separate 'silent flag'. I think checking
 * !dmc_counter suffices. */
static inline int dmc_silent_p (void) { return !SND.dmc_counter; }

void dmc_configure (void)
{
    SND.dmc_address = (nes.snd.regs[0x12] * 0x40) + 0xC000;
    SND.dmc_counter = (nes.snd.regs[0x13] * 0x10) + 1;
}

void dmc_read_next (void)
{
    if (SND.dmc_counter) {
        nes.cpu.Stolen += 4 * MASTER_CLOCK_DIVIDER;
        SND.dmc_buffer = Rd6502(SND.dmc_address);
        SND.dmc_address = ((SND.dmc_address + 1) & 0x7FFF) | 0x8000;
        SND.dmc_counter--;
        if (!SND.dmc_counter) {
            if (nes.snd.regs[0x10] & 0x40) dmc_configure();
            else if (nes.snd.regs[0x10] & 0x80) {
                nes.snd.regs[0x15] |= 0x80;
                signal_dmc_interrupt();
            }
        }
    }
}

void dmc_start_output_cycle (void)
{
    SND.dmc_shift_counter = 8;
    SND.dmc_shift_register = SND.dmc_buffer;
    dmc_read_next();
}

void dmc_clock (void)
{
    if (!SND.dmc_shift_counter) dmc_start_output_cycle();

    if (!dmc_silent_p()) {
        if ((SND.dmc_shift_register & 1) && (SND.dmc_dac < 126)) SND.dmc_dac += 2;
        else if (!(SND.dmc_shift_register & 1) && (SND.dmc_dac > 1)) SND.dmc_dac -= 2;
    }

    SND.dmc_shift_register >>= 1;
    SND.dmc_shift_counter--;
}

void clock_dmc_ptimer (void)
{
    SND.ptimer[4] -= CLOCK / 48000;
    while (SND.ptimer[4] < 0) {
        dmc_clock();
        SND.ptimer[4] += (SND.wavelength[4]+1);
    }
}

unsigned translate_dmc_period (unsigned period)
{
    unsigned periods[16] =
        { 0x1AC, 0x17C, 0x154, 0x140, 0x11E, 0x0FE, 0x0E2, 0x0D6,
          0x0BE, 0x0A0, 0x08E, 0x080, 0x06A, 0x054, 0x048, 0x036 };
    return periods[period];
}


/** Emulate write to sound register. Addr is 0..0x17 **/
void snd_write (unsigned addr, unsigned char value)
{
    int chan = addr >> 2;

    /* Generate audio up to the current point in time before modifying
     * register contents. */
    snd_catchup();

    SDL_mutexP(producer_mutex);

    if (0)
        printf("%ssnd %2X <- %02X  len(%2x,%2x,%2x,%2x) status=%02X vol(%i,%i,*,%i)\n", nes_time_string(), addr, value, SND.lcounter[0], SND.lcounter[1], SND.lcounter[2], SND.lcounter[3], nes.snd.regs[0x15], SND.volume[0], SND.volume[1], SND.volume[3]);

    switch (addr) {
    case 0x15: /* channel enable register */
        nes.snd.regs[0x15] = value & 0x5F; /* Clear DMC but not frame interrupt flag? */

        for (chan=0; chan<4; chan++)
            if (!(value & BIT(chan))) SND.lcounter[chan]=0;
        break;
    case 0x17:
        SND.frameseq_divider = frameseq_divider_reset;
        SND.frameseq_sequencer = 0;
        SND.frameseq_mode_5 = value & 0x80 ? 1 : 0;
        SND.frameseq_irq_disable = value & 0x40;
        if (value & 0x40) nes.snd.regs[0x15] &= 0xBF; /* clear frameseq irq flag */

        if (SND.frameseq_mode_5) {
            /* Okay, I think what he meant was that the *sequencer* should be clocked (not the divider) */
            frameseq_clock_sequencer();
        }
        break;

    /* Configure volume / envelope */
    case 0: case 4: case 12:
        //if (value & BIT(4)) SND.volume[chan] = value & 0x0F; /* Disable envelope decay */
        nes.snd.regs[addr] = value;
        envelope_compute_output_volume(chan);
        /*    printf("snd_write: reg %i val %02X\t\t volume %s%i, volume decay %s, volume looping %s\n",
              addr, value, PBIT(4,value) ? "" : "decay rate ", LDB(3,4,value),
              PBIT(4,value) ? "disabled" : "enabled",
              PBIT(5, value) ? "enabled" : "disabled"); */
        break;

    /* Configure sweep unit */
    case 1: case 5:
        nes.snd.regs[addr] = value;
        SND.sweep_reset[chan] = 1;
        break;

    /* Configure linear counter */
    case 8:
        //printf("%u: wrote %02X to $4008.\n", nes.time, value);
        nes.snd.regs[addr] = value;
        break;

    case 2: case 6: case 0x0A:
        SND.wavelength[chan]=(SND.wavelength[chan]&0xF00) | value;
        break;

    case 0x0B:
        SND.linear_counter = nes.snd.regs[8] & 0x7F;
        /*if (nes.snd.regs[8] & 0x80)*/ SND.linear_counter_halt = 1;
        //printf("%u: wrote %02X to $400B. $4008 currently %02X.\n", nes.time, value, nes.snd.regs[8]);

        /**** Fall through to case 3/7: ****/

    case 3: case 7:
        nes.snd.regs[addr] = value;
        SND.wavelength[chan]=(SND.wavelength[chan]&0x0FF) | ((value&0x07)<<8);
        SND.env_reset[chan] = 1;

        /* We must reset the output counter, otherwise various
         * "counting down" effects (such as at the end of a level in
         * SMB3, and the fairy pools in Zelda 1) don't sound right. */
        if (chan < 2) SND.out_counter[chan] = 0;


        if (nes.snd.regs[0x15] & BIT(chan)) {
            SND.lcounter[chan] = translate_length(value);

            /*
            printf("Loaded length counter %4X (%i) with %i, wrote %X (translated to %i)\n",
                   addr+0x4000, chan, SND.lcounter[chan], value, translate_length(value));
            */
        } //else printf("Not loading length counter because channel %i is disabled.\n", chan);
        break;

    case 0x0E:
        nes.snd.regs[addr] = value;
        SND.wavelength[3] = translate_noise_period(value & 15);
        break;

    case 0x0F:
        nes.snd.regs[addr] = value;
        SND.env_reset[chan] = 1;
        if (nes.snd.regs[0x15] & BIT(chan)) SND.lcounter[chan] = translate_length(value);
        break;

    /* Delta modulation registers */
    case 0x10:
        SND.wavelength[4] = translate_dmc_period(value & 15);
        nes.snd.regs[addr] = value;
        break;

    case 0x11:
        SND.dmc_dac = value & 0x7F;
        //printf("Direct write to DMC DAC: %i.\n", value);
        break;

    case 0x12:
    case 0x13:
        nes.snd.regs[addr] = value;
        dmc_configure();
        break;

    default: break;
    }

    SDL_mutexV(producer_mutex);
}

unsigned char snd_read_status_reg (void)
{
    unsigned char result = 0;
    int i;

    snd_catchup();

    SDL_mutexP(producer_mutex);
    for (i=0; i<4; i++) if (SND.lcounter[i] > 0) result |= (1 << i);
    if (SND.dmc_counter) result |= BIT(4);
    result |= nes.snd.regs[0x15] & 0xE0;
    /* apu_reg.txt says to clear frameseq interrupt flag on read.
       It doesn't say anything about the DMC interrupt flag here.    */
    nes.snd.regs[0x15] &= 0xBF;
    SDL_mutexV(producer_mutex);
    return result;
}

static inline void clock_ptimer (int channel, int mask)
{
    SND.ptimer[channel] -= CLOCK/48000;
    while (SND.ptimer[channel] < 0) {
        /* +1 here is definitely the right thing. You can hear it when
           the pitch gets very high, like the 1-up melody in SMB1. */
        SND.ptimer[channel] += (SND.wavelength[channel]+1);
        SND.out_counter[channel]++;
        SND.out_counter[channel] &= mask;
    }
}

/** Noise channel LFSR **/

unsigned lfsr = 1;

static inline void clock_lfsr (void)
{
    if (!(nes.snd.regs[14] & 0x80))
        lfsr = (lfsr >> 1) | (((lfsr&1) ^ ((lfsr & 2)>>1))<<14);
    else lfsr = (lfsr >> 1) | (((lfsr&1) ^ ((lfsr>>6)&1))<<14);
}

static inline void clock_noise_ptimer (void)
{
    SND.ptimer[3] -= CLOCK/48000;
    while (SND.ptimer[3] < 0) {
        SND.ptimer[3] += (SND.wavelength[3]+1);
        clock_lfsr();
    }
}

const int clocks_per_sample = 4467;


/* Generate audio. Call this with the producer lock held. */
void snd_render_samples (int emergency_mode, int samples)
{
    //printf("catchup: %i cycles since last call (%i samples).\n", delta, samples);
    if (samples > 0) {
        if (!sound_enabled) buffer_low = buffer_high = 0;

        while (samples) {
            unsigned space = buffer_space(), nfill = space;
            if (nfill > samples) nfill = samples;
            //printf("sound delta: %i cycles: %i samples. %i free in buffer. filling %i. low=%i, high=%i\n", delta, samples, space, nfill, buffer_low, buffer_high);
            snd_fillbuffer(audio_buffer, buffer_high & BUFFER_PTR_MASK, nfill);
            samples -= nfill;
            buffer_high = buffer_high + nfill;
            if (!emergency_mode) nes.last_sound_cycle += nfill * clocks_per_sample;

        }
    }
}

void snd_catchup (void)
{
    SDL_mutexP(producer_mutex);

    int delta = nes.cpu.Cycles - nes.last_sound_cycle;
    int samples = delta / clocks_per_sample;
    if (samples > 1000) {
        printf("%sWtf. samples = %i ??? (%i - %i)\n",
               nes_time_string(),
               samples,
               (int) nes.cpu.Cycles,
               (int) nes.last_sound_cycle);
    }

    if (samples && (delta > 0)) snd_render_samples(0, samples);

    service_interrupts();

    SDL_mutexV(producer_mutex);
}

static void snd_fillbuffer (Sint16 *buf, unsigned index, unsigned length)
{
    //const int dc_table[4] = { 14, 12, 8, 4 };
//    static float f_tri = 0.0, f_tri_param = 0.7;
    const int dc_table[4][8] =
        { { 0, 1, 0, 0, 0, 0, 0, 0 },
          { 0, 1, 1, 0, 0, 0, 0, 0 },
          { 0, 1, 1, 1, 1, 0, 0, 0 },
          { 1, 0, 0, 1, 1, 1, 1, 1 } };
    int fin = index + length;
    Sint16 mask = -1;
    if (sound_muted) mask = 0;
    //int num_neg = 0;
    while (index < fin) {
        int sq1 = 0, sq2 = 0, tri = 0, noise = 0, dmc = 0;
        Sint16 samp;

        frameseq_clock_divider();

        clock_ptimer(0, 0x0F);
        if (dc_table[nes.snd.regs[0]>>6][SND.out_counter[0]>>1] &&
            (SND.wavelength[0] >= 8) && SND.lcounter[0]) {
            sq1 = SND.volume[0];
        }

        clock_ptimer(1, 0x0F);
        if (dc_table[nes.snd.regs[4]>>6][SND.out_counter[1]>>1] &&
            (SND.wavelength[1] >= 8) && SND.lcounter[1]) {
            sq2 = SND.volume[1];
        }

        if (SND.lcounter[2] && (SND.wavelength[2] >=8 ) && SND.linear_counter) clock_ptimer(2, 0x1F);
        tri = (SND.out_counter[2] >= 0x10) ? SND.out_counter[2]^0x1F : SND.out_counter[2];

        clock_noise_ptimer();
        if (SND.lcounter[3]) {
            if (!(lfsr & 1)) noise = SND.volume[3];
        }

        clock_dmc_ptimer();
        /* Note that bit 4 only disables the DMA unit, not the DAC. */
        dmc = SND.dmc_dac;

        //f_tri = f_tri*f_tri_param + ((float)tri)*(1.0-f_tri_param);

        sfloat_t tmp =
         28000.0 * ((159.79 / (100.0 + 1.0 / ( ((float)tri) / 8227.0 +
                                               ((float)noise) / 12241.0 +
                                               ((float)dmc) / 22638.0)))
                          +
                          ((sq1 | sq2)? 95.88 / (100.0 + 8128.0 / (sq1 + sq2)) : 0.0));

        /*
        if (!(index & 0x3FF) && (SND.linear_counter_halt && SND.linear_counter))
            printf("%8i %8i  %16.8f %16.8f\n", buf[index], SND.linear_counter, tmp, SND.filter_accumulator);
        */

        const sfloat_t decay = 0.999;
        SND.filter_accumulator = (SND.filter_accumulator - tmp) * decay + tmp;
        //if ((tmp - SND.filter_accumulator) < 0.0) num_neg++;

        samp = tmp - SND.filter_accumulator;
        samp &= mask;

        buf[index & ~AUDIO_BUFFER_SIZE] = samp;
        buf[index | AUDIO_BUFFER_SIZE] = samp;
        index++;

        //printf("%X %X %02X %X %02X => %5i\n", sq1, sq2, tri, noise, dmc, buf[index]);
        //printf("%8i ", buf[index]);

    }

    /*
    if (length > 20)
        printf("%5i  %3i  %4i / %4i  %8f\n",
               length, SND.linear_counter, num_neg,
               length - num_neg, SND.filter_accumulator);
    */
}
