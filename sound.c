#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <SDL/SDL.h>
#include <signal.h>
#include <math.h>
#include "sys.h"
#include "config.h"
#include "nes.h"
#include "sound.h"

/* TODO: Move audio state out of random global variables and into the NES struct.   
   
   FIXME? I was really a lot better off when the audio was generated
   on demand within the audio callback. Now I've got this huge
   headache of having to introduce extra buffering and struggling to
   keep the emulator thread producing at just the right rate.

   FIXME: Audio frequencies are off by a bit due to rounding of CLOCK/48000.

   FIXME: Interrupt acknowledgement. Right now we fire interrupts once, rather
          than emulating the real behavior of the interrupt signals, so it's
          possible to lose interrupts if e.g. the frameseq and DMC IRQs occur
          simultaneously.
*/

#define AUDIO_BUFFER_SIZE 4096
#define BUFFER_PTR_MASK   (AUDIO_BUFFER_SIZE-1)

/* Audio is output to a mirrored circular buffer with low/high water mark */
volatile int buffer_low = 0;
volatile int buffer_high = 0;
static Sint16 audio_buffer[AUDIO_BUFFER_SIZE*2];

int rate_governor = 0; /* tuned for my laptop */

int sound_initialized = 0;
volatile int sound_enabled = 1;

static void buffer_init (void);
static void audio_callback (void *udata, Sint16 * stream, int len);
static void snd_fillbuffer (Sint16 * buf, unsigned index, unsigned length);

static SDL_AudioSpec desired, obtained;

int snd_init (void)
{
    if (sound_globalenabled) {
        int i;

        buffer_init();

        desired.freq = 48000;
        desired.format = AUDIO_S16 /* | AUDIO_MONO */ ;
        desired.samples = 512; /* Desired buffer size */
        desired.callback = audio_callback;
        desired.channels = 1;
        desired.userdata = NULL;

        printf ("Opening audio device.\n");
        if (SDL_OpenAudio (&desired, &obtained)) {
            printf ("SDL_OpenAudio failed: %s", SDL_GetError ());
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

}

/* buffer_samples: Returns used space in the audio buffer */
static int buffer_samples (void)
{
    return buffer_high - buffer_low;
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
    buffer_high = AUDIO_BUFFER_SIZE / 2;
}

const int desired_buffer_ahead = 1200;
int first_buffer = 1;
int delta_log[16];
int fill_log[16];
int delta_log_idx = 0;


static void audio_callback (void *udata, Sint16 *stream, int len)
{ 
    int req = len >> 1, num = buffer_samples(), consumed = req, need = num-req > 0? 0 : req-num;
    int ideal_buffer_length = req + desired_buffer_ahead;
    static int last = 0;

    /* There appears to be some considerable delay between starting up
     * the audio system and the first invocation of the callback, such
     * that there is more audio buffered than you'd expect. Therefore,
     * snap to the end of the buffer, minus however much we'd prefer
     * to keep as a safety margin. */
    if (first_buffer) {
        while (buffer_samples() < ideal_buffer_length) { /* Wait. */ }
        buffer_low = buffer_high - ideal_buffer_length - 300;
        num = buffer_samples();
        first_buffer = 0;
        memset(delta_log, 0, sizeof(delta_log));
        memset(fill_log, 0, sizeof(delta_log));
    } else {
        int i, avg_x = 0, avg_dx = 0;
        delta_log[delta_log_idx] = num - last;
        fill_log[delta_log_idx] = num;
        delta_log_idx = (delta_log_idx + 1) % 16;

        for (i=0; i<16; i++) { avg_dx += delta_log[i]; avg_x += fill_log[i]; }
        avg_x /= i;
        avg_dx /= i;
        if (!delta_log_idx) {
            //printf("Average drift: %4i, average fill: %4i (gov=%i), frob=%i\n", avg_dx, avg_x, rate_governor, ((avg_x - ideal_buffer_length) / 24));
            //rate_governor += max(-3,min(3,((avg_x - ideal_buffer_length) / 64)));
            //rate_governor += (avg_dx / 32);

        }
    }

    if (num < (ideal_buffer_length - req/4)) rate_governor--;
    if (num > (ideal_buffer_length + req/4)) rate_governor++;

    /* Maybe we can get away with this.. */
    //while (sound_enabled && (buffer_samples() < req)) ;
    //printf("Waiting! requested %i, %i available\n", req, buffer_samples());

    //printf("Audio callback requested %i. %i available (%i free)\n", req, buffer_samples(), buffer_space());
    //printf("%i\n", num - last);
    last = num;
    
    if (num == AUDIO_BUFFER_SIZE) {
        //rate_governor++;
        printf("Buffer full. Rate governor is %i.\n", rate_governor);
    }

    if (sound_enabled) memcpy(stream, audio_buffer + (buffer_low & BUFFER_PTR_MASK), len);

    if (req > num) {
        //consumed = num / 2;     /* Why not? */
        consumed = 0;
        //if (rate_governor) rate_governor--;
        memset(delta_log, 0, sizeof(delta_log));
        printf("Underrun! requested %i, %i available. Rate governor adjusted to %i\n", req, num, rate_governor);
    }

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


int ptimer[5]={0,0,0,0,0};   /* 11 bit down counter */
int wavelength[5]={0,0,0,0,0}; /* controls ptimer */


int out_counter[3]={0,0,0}; /* for square and triangle channels.. */
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
int frameseq_divider = 200; //frameseq_divider_reset;
int frameseq_sequencer = 0;
int frameseq_mode_5 = 0;
int frameseq_irq_disable = 1;

#define FRAMESEQ_CLOCK_ENV_LIN 1
#define FRAMESEQ_CLOCK_LEN_SWEEP 2
#define FRAMESEQ_CLOCK_IRQ 4

int frameseq_patterns[2][5] = { { 1, 3, 1, 7, 0}, { 3, 1, 3, 1, 0} };

/* Clock the frameseq divider. Should be called at 48000 Hz. */
void frameseq_clock_divider (void) {
    if (!frameseq_divider) {
        frameseq_clock_sequencer();
    } else frameseq_divider--;
}

/* Clock the sequencer. Called when divider counts down. */
void frameseq_clock_sequencer (void)
{
        int output;
        frameseq_divider = frameseq_divider_reset;

        output = frameseq_patterns[frameseq_mode_5][frameseq_sequencer];
        if ((output & FRAMESEQ_CLOCK_IRQ) && !frameseq_irq_disable) {
            nes.snd.regs[0x15] |= 0x40;
            //printf("%u.%u: frame sequencer interrupt.\n", frame_number, nes.scanline);
            Int6502(&nes.cpu, INT_IRQ);
        }

        if (output & FRAMESEQ_CLOCK_LEN_SWEEP) {
            length_counters_clock();
            sweep_clock();
        }
        
        if (output & FRAMESEQ_CLOCK_ENV_LIN) {
            envelope_clock();
            linear_counter_clock();
        }

        frameseq_sequencer++;
        if ((frameseq_sequencer == 5) ||
            ((frameseq_sequencer == 4) && !frameseq_mode_5)) frameseq_sequencer = 0;
}

/** Length counters count down unless the hold bit is set. **/

int lcounter[4]={0,0,0,0}; /* 7 bit down counter */

inline int tick (int value) { return value? value-1 : 0; }

void length_counters_clock (void)
{
    byte *r=nes.snd.regs;

    if ( !(r[0] & 0x20)) lcounter[0] = tick(lcounter[0]);
    if ( !(r[4] & 0x20)) lcounter[1] = tick(lcounter[1]);
    if ( !(r[8] & 0x80)) lcounter[2] = tick(lcounter[2]);
    if (!(r[12] & 0x20)) lcounter[3] = tick(lcounter[3]);
}

/** Sweep unit **/

int sweep_reset[2] = {0,0};
int sweep_divider[2] = {0,0};

void sweep_clock_channel (int channel)
{
    int reg = nes.snd.regs[channel*4+1],
        enabled = reg & 0x80,
        period = ((reg>>4) & 7) + 1,
        negate = reg & 8,
        shift = reg & 7;
    int wl = wavelength[channel];

    assert(sweep_divider[channel] >= 0);
    
    // printf("%u: sweep clocked for channel %x. sweep reg=%02X. divider=%i\n", frame_number, channel, reg, sweep_divider[channel]);

    /* Clock the divider */
    if (!sweep_divider[channel]) {
        int shifted = wl >> shift;
        int sum = wl + (negate? (-shifted - (channel^1)) : shifted);


        if (0 && channel == 0) {
            printf("%u: ch0 sweep enabled: %i, period = %x, negate = %x, shift=%x   wl=%i shifted=%i  LC=%i\n",
                   frame_number, enabled?1:0, period, negate, shift, wl, shifted, lcounter[0]);
        }


        if (enabled && (shift>0) && (wl >= 8) && (sum <= 0x7FF)) wavelength[channel] = sum;
        if (wavelength[channel] < 0) {
            wavelength[channel] = 0;
            printf("%u: Sweep underflow on channel %i. (?)\n", frame_number, channel);
        }
        
        /* The sweep can silence the channel. Exactly how is not
           mentioned, so I elect to zero the length counter. We only
           need to do this on carry, for sweep up, because a sweep
           down will eventually drop the wavelength below 8, at which
           time the channel is muted at the output stage. */
        if (sum >= 0x800) lcounter[channel] = 0;

        sweep_divider[channel] = period;
    } else sweep_divider[channel]--;
    
    /* Check for reset */
    if (sweep_reset[channel]) {
        sweep_reset[channel] = 0;
        sweep_divider[channel] = period;
        //printf(" sweep reset channel %x divider to %X\n", channel, period);
    }
}

void sweep_clock (void)
{
    sweep_clock_channel(0);
    sweep_clock_channel(1);
}

/** Envelope counter **/

int envc_divider[4] = {0,0,0,0};
int envc[4] = {0,0,0,0};
int env_reset[4] = {0,0,0,0};
/* We latch the output of the envelope/constant volume here rather
 * than having to branch on the envelope disable flag for every sample
 * of output. */
int volume[4] = {0,0,0,0};

inline void envelope_counter_clock (int channel)
{
    if (envc[channel]) envc[channel]--;
    else if (nes.snd.regs[channel<<2] & 0x20) envc[channel] = 15;    
    if (!(nes.snd.regs[channel<<2] & 0x10)) volume[channel] = envc[channel];
}

void envelope_clock (void)
{
    int i;

    for (i=0; i<4; i++) {
        if (env_reset[i]) {     /* Reset divider? */
            env_reset[i] = 0;
            envc[i] = 15;
            envc_divider[i] = (nes.snd.regs[i<<2] & 0x0F) + 1;
        } else {                /* Clock divider. */
            if (!envc_divider[i]) {
                envc_divider[i] = (nes.snd.regs[i<<2] & 0x0F) + 1;
                envelope_counter_clock(i);
            } else envc_divider[i]--;
        }
    }
}

/** Linear Counter (triangle channel) **/
int linear_counter = 0;
int linear_counter_halt = 0;
void linear_counter_clock (void)
{
    // printf("%u: linear_counter clocked. currently %i. halted? %s.regs = %02X %02X\n", frame_number, linear_counter, linear_counter_halt? "Yes":"No", nes.snd.regs[8], nes.snd.regs[11]);
    if (linear_counter_halt) {
        linear_counter = nes.snd.regs[8] & 0x7F;
        // printf("  %u: Linear counter took new value %02X.\n", frame_number, linear_counter);
    } else if (linear_counter) linear_counter--;
    
    if (!(nes.snd.regs[8]&0x80)) linear_counter_halt = 0;
}

int translate_noise_period (int n)
{
    const int periods[16] = { 0x004, 0x008, 0x010, 0x020, 0x040, 0x060, 
                              0x080, 0x0A0, 0x0CA, 0x0FE, 0x17C, 0x1FC, 
                              0x2FA, 0x3F8, 0x7F2, 0xFE4 };
    return periods[n];
}

/** Delta Modulation Channel **/

unsigned dmc_address = 0x8000;
unsigned dmc_counter = 0;
byte dmc_buffer = 0;
int dmc_dac = 0;
int dmc_shift_counter = 0;
int dmc_shift_register = 0;

/* We don't have a separate 'silent flag'. I think checking
 * !dmc_counter suffices. */
inline int dmc_silent_p (void) { return !dmc_counter; }

void dmc_configure (void)
{
    dmc_address = (nes.snd.regs[0x12] * 0x40) + 0xC000;
    dmc_counter = (nes.snd.regs[0x13] * 0x10) + 1;
}

void dmc_read_next (void)
{
    if (dmc_counter) {
        dmc_buffer = Rd6502(dmc_address);
        dmc_address = ((dmc_address + 1) & 0x7FFF) | 0x8000;
        dmc_counter--;
        if (!dmc_counter) {
            if (nes.snd.regs[0x10] & 0x40) dmc_configure();
            else if (nes.snd.regs[0x10] & 0x80) {
                nes.snd.regs[0x15] |= 0x80;
                Int6502(&nes.cpu, INT_IRQ);
            }
        }
    }
}

void dmc_start_output_cycle (void)
{
    dmc_shift_counter = 8;
    dmc_shift_register = dmc_buffer;
    dmc_read_next();
}

void dmc_clock (void)
{
    if (!dmc_shift_counter) dmc_start_output_cycle();

    if (!dmc_silent_p()) {
        if ((dmc_shift_register & 1) && (dmc_dac < 126)) dmc_dac += 2;
        else if (!(dmc_shift_register & 1) && (dmc_dac > 1)) dmc_dac -= 2;        
    }
    
    dmc_shift_register >>= 1;
    dmc_shift_counter--;
}

void clock_dmc_ptimer (void)
{
    ptimer[4] -= CLOCK / 48000;
    while (ptimer[4] < 0) {
        dmc_clock();
        ptimer[4] += (wavelength[4]+1);
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

    if (addr == 0x16) return;

    if ((addr < 4) || (addr == 0x15))
        printf("%u.%u: snd %2X <- %02X  (%2x/%2x/%2x/%2x) status=%02X\n", frame_number, nes.scanline, addr, value, lcounter[0], lcounter[1], lcounter[2], lcounter[3], nes.snd.regs[0x15]); 

    switch (addr) {
    case 0x15: /* channel enable register */
        nes.snd.regs[0x15] = value & 0x5F; /* Clear DMC but not frame interrupt flag? */

        for (chan=0; chan<4; chan++)
            if (!(value & BIT(chan))) lcounter[chan]=0;        
        break;
    case 0x17:
        frameseq_divider = frameseq_divider_reset;
        frameseq_sequencer = 0;
        frameseq_mode_5 = value & 0x80 ? 1 : 0;
        frameseq_irq_disable = value & 0x40;
        if (value & 0x40) nes.snd.regs[0x15] &= 0xBF; /* clear frameseq irq flag */
        
        if (frameseq_mode_5) {
            /* Okay, I think what he meant was that the *sequencer* should be clocked (not the divider) */
            frameseq_clock_sequencer();
        }
        break;

    /* Configure volume / envelope */
    case 0: case 4: case 12:
        if (value & BIT(4)) volume[chan] = value & 0x0F;
        nes.snd.regs[addr] = value;
        /*    printf("snd_write: reg %i val %02X\t\t volume %s%i, volume decay %s, volume looping %s\n", 
              addr, value, PBIT(4,value) ? "" : "decay rate ", LDB(3,4,value),
              PBIT(4,value) ? "disabled" : "enabled", 
              PBIT(5, value) ? "enabled" : "disabled"); */
        break;

    /* Configure sweep unit */
    case 1: case 5:
        nes.snd.regs[addr] = value;
        sweep_reset[chan] = 1;
        break;

    /* Configure linear counter */
    case 8:
        //printf("%u: wrote %02X to $4008.\n", frame_number, value);
        nes.snd.regs[addr] = value;
        break;

    case 2: case 6: case 0x0A:
        wavelength[chan]=(wavelength[chan]&0xF00) | value;
        break;

    case 0x0B:
        linear_counter = nes.snd.regs[8] & 0x7F;
        /*if (nes.snd.regs[8] & 0x80)*/ linear_counter_halt = 1;
        //printf("%u: wrote %02X to $400B. $4008 currently %02X.\n", frame_number, value, nes.snd.regs[8]);
        /* fall through to case 3/7: */
    case 3: case 7:
        nes.snd.regs[addr] = value;
        wavelength[chan]=(wavelength[chan]&0x0FF) | ((value&0x07)<<8);
        env_reset[chan] = 1;
        /* Don't reset the output counters. It creates clicks/pops in the audio. */
        if (nes.snd.regs[0x15] & BIT(chan)) {
            lcounter[chan] = translate_length(value);
            /*printf("%u: Loaded length counter %4X (%i) with %i, wrote %X (translated to %i)\n",
              frame_number, addr+0x4000, chan, lcounter[chan], value, translate_length(value));  */
                        
        }
        break;

    case 0x0E:
        nes.snd.regs[addr] = value;
        wavelength[3] = translate_noise_period(value & 15);
        break;

    case 0x0F:
        nes.snd.regs[addr] = value;
        env_reset[chan] = 1;
        if (nes.snd.regs[0x15] & BIT(chan)) lcounter[chan] = translate_length(value);
        break;

    /* Delta modulation registers */
    case 0x10:
        wavelength[4] = translate_dmc_period(value & 15);
        nes.snd.regs[addr] = value;
        break;

    case 0x11:
        dmc_dac = value & 0x7F;
        //printf("Direct write to DMC DAC: %i. Enabled? %i\n", value, nes.snd.regs[0x15] & BIT(4));
        break;

    case 0x12:
    case 0x13:
        nes.snd.regs[addr] = value;
        dmc_configure();
        break;        

    default: break;
    }
}

unsigned char snd_read_status_reg (void)
{
    unsigned char result = 0;
    int i;
    snd_catchup();
    for (i=0; i<4; i++) if (lcounter[i] > 0) result |= (1 << i);
    if (dmc_counter) result |= BIT(4);
    result |= nes.snd.regs[0x15] & 0xE0;
    /* apu_reg.txt says to clear frameseq interrupt flag on read.
       It doesn't say anything about the DMC interrupt flag here.    */
    nes.snd.regs[0x15] &= 0xBF; 
    return result;
}

inline void clock_ptimer (int channel, int mask)
{
    ptimer[channel] -= CLOCK/48000;
    while (ptimer[channel] < 0) {
        /* +1 here is definitely the right thing. You can hear it when
           the pitch gets very high, like the 1-up melody in SMB1. */
        ptimer[channel] += (wavelength[channel]+1);
        out_counter[channel]++;
        out_counter[channel] &= mask;
    }
}

/** Noise channel LFSR **/

unsigned lfsr = 1;

inline void clock_lfsr (void)
{
    if (!(nes.snd.regs[14] & 0x80))
        lfsr = (lfsr >> 1) | (((lfsr&1) ^ ((lfsr & 2)>>1))<<14);
    else lfsr = (lfsr >> 1) | (((lfsr&1) ^ ((lfsr>>6)&1))<<14);
}

inline void clock_noise_ptimer (void)
{
    ptimer[3] -= CLOCK/48000;
    while (ptimer[3] < 0) {
        ptimer[3] += (wavelength[3]+1);
        clock_lfsr();
    }
}

void snd_catchup (void)
{
    int delta = nes.cpu.Cycles - nes.last_sound_cycle;
    int cpu_per_sample = 4474 ; //+ max(-1000, rate_governor); /* 447.443 = 21.47727 MHz / 48000 Hz */
    int samples = delta / cpu_per_sample;
    int prev_space = -1;
    /* Awesome, now we have two passes of rounding error affecting the audio frequency.. */

    //printf("catchup: %i cycles since last call (%i samples).\n", delta, samples);
    if (samples > 0) {
        if (!sound_enabled) buffer_low = buffer_high = 0;

        while (samples) {
            unsigned space = buffer_space(), nfill = space;
            //if (prev_space != -1) printf("waiting (%i samples, space=%i, prev_space=%i)..\n", samples, space, prev_space);
            if (nfill > samples) nfill = samples;
            //printf("sound delta: %i cycles: %i samples. %i free in buffer. filling %i. low=%i, high=%i\n", delta, samples, space, nfill, buffer_low, buffer_high);
            snd_fillbuffer(audio_buffer, buffer_high & BUFFER_PTR_MASK, nfill);
            samples -= nfill;
            buffer_high = buffer_high + nfill;
            nes.last_sound_cycle += nfill * cpu_per_sample;
            prev_space = space;
        }
    }
}

static void snd_fillbuffer (Sint16 *buf, unsigned index, unsigned length)
{
    //const int dc_table[4] = { 14, 12, 8, 4 };
    static float f_tri = 0.0, f_tri_param = 0.7;
    const int dc_table[4][8] = 
        { { 0, 1, 0, 0, 0, 0, 0, 0 },
          { 0, 1, 1, 0, 0, 0, 0, 0 },
          { 0, 1, 1, 1, 1, 0, 0, 0 },
          { 1, 0, 0, 1, 1, 1, 1, 1 } };    
    int fin = index + length;

    while (index < fin) {
        int sq1 = 0, sq2 = 0, tri = 0, noise = 0, dmc = 0;
        Sint16 samp;
        frameseq_clock_divider();

        clock_ptimer(0, 0x0F);
        if (dc_table[nes.snd.regs[0]>>6][out_counter[0]>>1] && 
            (wavelength[0] >= 8) && lcounter[0]) {
            sq1 = volume[0];
        }

        clock_ptimer(1, 0x0F);
        if (dc_table[nes.snd.regs[4]>>6][out_counter[1]>>1] &&
            (wavelength[1] >= 8) && lcounter[1]) {
            sq2 = volume[1];
        }

        if (lcounter[2] && (wavelength[2] >=8 ) && linear_counter) clock_ptimer(2, 0x1F);
        tri = (out_counter[2] >= 0x10) ? out_counter[2]^0x1F : out_counter[2];
        
        clock_noise_ptimer();
        if (lcounter[3]) {
            if (lfsr & 1) noise = volume[3];
        }

        clock_dmc_ptimer();
        /* Not that bit 4 only disable the DMA unit, not the DAC. */
        dmc = dmc_dac;
        
        //f_tri = f_tri*f_tri_param + ((float)tri)*(1.0-f_tri_param);
        
        samp = 30000.0 * ((159.79 / (100.0 + 1.0 / ( ((float)tri) / 8227.0 +
                                                     ((float)noise) / 12241.0 +
                                                     ((float)dmc) / 22638.0)))
                          +
                          ((sq1 | sq2)? 95.88 / (100.0 + 8128.0 / (sq1 + sq2)) : 0.0));
        // Alternatively, without fancy (and slow) channel mixing:
        // samp = (sq1 + sq2 + tri + noise) * 512;

        samp = sq1  * 1000;

        buf[index & ~AUDIO_BUFFER_SIZE] = samp;
        buf[index | AUDIO_BUFFER_SIZE] = samp;
        index++;

        //printf("%X %X %02X %X %02X => %5i\n", sq1, sq2, tri, noise, dmc, buf[i]);
             
    }
}

