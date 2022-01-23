#include <EEPROM.h>
#include <LedControl.h>


/*
EUGENE Euclidean Rhythm Generator
2022-01-23 Alessio Nuti
Release Notes:
- 4 channels
- all encoders working (1 = length, 2 = hits, 3 = offset)
- all switches working (1 = switch channel, 2 = enter clock divider controlled by potentiometer, 3 = sleep display)
- clock managed by interrupts
- euclidean algorithm rewritten
- UI with scrolling display (1 line per channel), and a common area for displaying active changes
To do:
- implement per-channel clock divider
- improve display layout
BUGS:
- crash at startup after some cycles, then ok
*/


#define ENC_1A 6  // n / length
#define ENC_1B 5
#define ENC_2A 8  // k / beats
#define ENC_2B 7
#define ENC_3A 10  // o / offset
#define ENC_3B 9
#define ENC_SWITCH A2
#define OUT_1 A3
#define OUT_2 11
#define OUT_3 12
#define OUT_4 13
#define LED_DIN 4
#define LED_CLK 3
#define LED_LOAD A0
#define CLK_IN 2
#define RESET_IN A1
#define RESET_BUT A4
#define POT A5

#define BRIGHTNESS 4
#define DEBUG 0                // 0 = normal / 1 = internal clock / 2= serial debug
#define DISPLAY_TIMEOUT 2000   // ms
#define ENC_DEBOUNCE 50        // ms
#define SWITCH_DEBOUNCE 200    // ms
#define MAX_PULSE_LENGTH 1000  // ms

// constants
#define N_CHANNELS 4   // number of channels
#define MAX_LENGTH 16  // max sequence length
#define MIN_LENGTH 1   // min sequence length

LedControl lc = LedControl(LED_LOAD, LED_CLK, LED_DIN, 1);
const byte OUT_PINS[N_CHANNELS] = {OUT_1, OUT_2, OUT_3, OUT_4};
bool diga_old;  // for encoders
bool digb_old;
int nknob;
int kknob;
int oknob;
int pot_val;
int last_pot_val;
unsigned long time = 0;
unsigned long last_sync = 0;
unsigned long last_read = 0;
unsigned long last_switch = 0;
bool sleep = true;
bool sleep_switch = false;
int ch_active = 0;  // channel active, zero indexed
int mode = 0;       // mode active 0 = normal / 1 = length / 2 = beats / 3 = clockdivider

volatile byte master_clock = 0;
volatile byte n_length[N_CHANNELS];   // length of the sequence
volatile byte k_hits[N_CHANNELS];     // number of hits in the sequence
volatile byte o_offset[N_CHANNELS];   // off-set of the sequence
volatile byte clock_div[N_CHANNELS];  // clock division
volatile byte curr_step[N_CHANNELS];  // current step
volatile bool sequence[N_CHANNELS][MAX_LENGTH];
volatile bool reset = false;
volatile bool pulses_active = false;
volatile unsigned long last_pulse = 0;

bool tosync = true;

void setup() {
    pinMode(ENC_1A, INPUT_PULLUP);
    pinMode(ENC_1B, INPUT_PULLUP);
    pinMode(ENC_2A, INPUT_PULLUP);
    pinMode(ENC_2B, INPUT_PULLUP);
    pinMode(ENC_3A, INPUT_PULLUP);
    pinMode(ENC_3B, INPUT_PULLUP);
    pinMode(OUT_1, OUTPUT);
    pinMode(OUT_2, OUTPUT);
    pinMode(OUT_3, OUTPUT);
    pinMode(OUT_4, OUTPUT);

    if (DEBUG == 2) {
        Serial.begin(9600);
    }

    if (EEPROM.read(255) != 127 || EEPROM.read(256) != 128) {  // if EEPROM is blank / corrupted, write some startup amounts
        EEPROM.write(0, 8);                                    // 1n
        EEPROM.write(1, 8);                                    // 2n
        EEPROM.write(2, 8);                                    // 3n
        EEPROM.write(3, 8);                                    // 4n
        EEPROM.write(4, 4);                                    // 1k
        EEPROM.write(5, 4);                                    // 2k
        EEPROM.write(6, 4);                                    // 3k
        EEPROM.write(7, 4);                                    // 4k
        EEPROM.write(8, 0);                                    // 1o
        EEPROM.write(9, 0);                                    // 2o
        EEPROM.write(10, 0);                                   // 3o
        EEPROM.write(11, 0);                                   // 4o
        EEPROM.write(255, 127);                                // two values to check if EEPROM has been initialized correctly
        EEPROM.write(256, 128);
    }

    for (int i = 0; i < N_CHANNELS; i++) {
        n_length[i] = EEPROM.read(i);
        k_hits[i] = EEPROM.read(i + 4);
        o_offset[i] = EEPROM.read(i + 8);
        clock_div[i] = 1;
        curr_step[i] = 0;
    }
    updateSequence();

    // set up Arduino interrupt
    attachInterrupt(digitalPinToInterrupt(CLK_IN), clock_isr, CHANGE);

    // The MAX72XX is in power-saving mode on startup, we have to do a wakeup call
    lc.shutdown(0, false);
    lc.setIntensity(0, BRIGHTNESS);
    lc.clearDisplay(0);
    sleep = false;
    wakeanim();
}

void loop() {
    time = millis();

    // HANDLE RESET
    reset = (digitalRead(RESET_BUT) == HIGH || digitalRead(RESET_IN) == LOW);

    // SLEEP ROUTINE
    if (!sleep && sleep_switch) {
        sleepanim();
        lc.shutdown(0, true);
        sleep = true;
    }
    if (sleep && !sleep_switch) {
        lc.clearDisplay(0);
        lc.shutdown(0, false);
        wakeanim();
        sleep = false;
    }

    if (time - last_read > DISPLAY_TIMEOUT) {
        mode = 0;
    }

    // READ N KNOB
    nknob = encoderRead(ENC_1A, ENC_1B);
    if (nknob != 0 && time - last_read > ENC_DEBOUNCE) {
        n_length[ch_active] = constrain((int)n_length[ch_active] + nknob, MIN_LENGTH, MAX_LENGTH);
        k_hits[ch_active] = constrain((int)k_hits[ch_active], 0, n_length[ch_active]);
        EEPROM.write(ch_active, n_length[ch_active]);
        EEPROM.write(ch_active + 4, k_hits[ch_active]);
        updateSequence();
        mode = 1;
        updateLedsMode1();
        last_read = millis();
    }

    // READ K KNOB
    kknob = encoderRead(ENC_2A, ENC_2B);
    if (kknob != 0 && time - last_read > ENC_DEBOUNCE) {
        k_hits[ch_active] = constrain((int)k_hits[ch_active] + kknob, 0, n_length[ch_active]);  // update with encoder reading
        EEPROM.write(ch_active + 4, k_hits[ch_active]);
        updateSequence();
        mode = 2;
        updateLedsMode2();
        last_read = millis();
    }

    // READ O KNOB
    oknob = encoderRead(ENC_3A, ENC_3B);
    if (oknob != 0 && time - last_read > ENC_DEBOUNCE) {
        o_offset[ch_active] += (oknob + n_length[ch_active]);
        o_offset[ch_active] %= n_length[ch_active];  // update with encoder reading
        EEPROM.write(ch_active + 8, o_offset[ch_active]);
        updateSequence();
        mode = 2;
        updateLedsMode2();
        last_read = millis();
    }

    // READ SWITCHES
    switchRead();

    // READ POTENTIOMETER
    pot_val = analogRead(POT);
    int pot_scaled = floor(pot_val / 1024.0 * 16.0) + 1;
    if (mode == 3) {
        if (abs(pot_val - last_pot_val) > 10 && abs(clock_div[ch_active] - pot_scaled) < 2) {
            clock_div[ch_active] = pot_scaled;
            last_pot_val = pot_val;
            last_read = millis();
        }
        updateLedsMode3();
    }

    // FINISH ANY PULSES EXCEEDING MAX_PULSE_LENGTH
    if (millis() - last_pulse > MAX_PULSE_LENGTH && pulses_active == true) {
        for (int i = 0; i < N_CHANNELS; i++) {
            digitalWrite(OUT_PINS[i], LOW);
        }
        pulses_active = false;
    }

    updateLeds();
}

int switchRead() {
    // read encoder switches
    int switch_read = analogRead(ENC_SWITCH);
    int channel_switch = -1;
    if (switch_read > 120 && switch_read < 205 && time - last_switch > SWITCH_DEBOUNCE) {
        sleep_switch = !sleep_switch;  // switch ENC3 = sleep
        last_switch = millis();
    };
    if (switch_read > 205 && switch_read < 350 && time - last_switch > SWITCH_DEBOUNCE) {
        mode = 3;  // switch ENC2 = clock division mode
        last_switch = millis();
        last_read = millis();
    };
    if (switch_read > 350 && switch_read < 550 && time - last_switch > SWITCH_DEBOUNCE) {
        ch_active = (ch_active + 1) % N_CHANNELS;  // switch ENC2 = active channel
        last_switch = millis();
    };
}

int encoderRead(int apin, int bpin) {
    /* function to read encoders at the designated pins
     returns +1, 0 or -1 dependent on direction f
     Contains no internal debounce, so calls should be delayed
     */
    bool diga = digitalRead(apin);
    bool digb = digitalRead(bpin);
    int result = 0;
    if (diga == diga_old && digb == digb_old) {
        result = 0;
    } else if (diga == true && digb == false) {
        result = 1;
        diga_old = diga;
        digb_old = digb;
    } else if (diga == false && digb == true) {
        result = -1;
        diga_old = diga;
        digb_old = digb;
    } else if (diga == false && digb == false && diga_old == true && digb_old == false) {
        result = 1;
        diga_old = diga;
        digb_old = digb;
    } else if (diga == false && digb == false && diga_old == false && digb_old == true) {
        result = -1;
        diga_old = diga;
        digb_old = digb;
    } else if (diga == false && digb == false) {
        result = 0;
        diga_old = diga;
        digb_old = digb;
    }
    return result;
}

void wakeanim() {
    for (int a = 0; a < 4; a++) {
        setRowCorr(3 - a, 255);
        setRowCorr(4 + a, 255);
        delay(50);
        setRowCorr(3 - a, 0);
        setRowCorr(4 + a, 0);
    }
}

void sleepanim() {
    for (int a = 0; a < 4; a++) {
        setRowCorr(a, 255);
        setRowCorr(7 - a, 255);
        delay(100);
        setRowCorr(a, 0);
        setRowCorr(7 - a, 0);
    }
}

void updateSequence() {
    for (int i = 0; i < N_CHANNELS; i++) {
        // see: https://www.computermusicdesign.com/simplest-euclidean-rhythm-algorithm-explained/
        int counter;
        if (k_hits[i] == 0)
            counter = 0;
        else
            counter = n_length[i] - k_hits[i];

        for (int j = 0; j < n_length[i]; j++) {
            int k = (j + o_offset[i]) % n_length[i];
            counter += k_hits[i];
            sequence[i][k] = (counter >= n_length[i]);
            counter %= n_length[i];
        }
        for (int j = n_length[i]; j < MAX_LENGTH; j++) {
            sequence[i][j] = false;
        }
    }
}

void clock_isr() {
    if (!digitalRead(CLK_IN)) {  // rising edge on input
        for (int i = 0; i < N_CHANNELS; i++) {
            // update current step
            if (reset) {
                curr_step[i] = 0;
            } else {
                curr_step[i] = (curr_step[i] + 1) % n_length[i];
            }
            // set outputs
            if (sequence[i][curr_step[i]])
                digitalWrite(OUT_PINS[i], HIGH);
            else
                digitalWrite(OUT_PINS[i], LOW);
        }
        if (reset) {
            master_clock = 0;
        } else {
            master_clock = (master_clock + 1) % 8;
        }
        pulses_active = true;
        last_pulse = millis();
    } else {  // falling edge on input
        for (int i = 0; i < N_CHANNELS; i++) {
            digitalWrite(OUT_PINS[i], LOW);
        }
        pulses_active = false;
    }
}

void setLedCorr(int row, int col, bool state) {
    lc.setLed(0, 7 - row, 7 - col, state);
}

void setRowCorr(int row, byte val) {
    lc.setRow(0, 7 - row, val);
}

void setChLed(int i, bool state) {
    setLedCorr(5, 2 * i, state);
    setLedCorr(5, 2 * i + 1, state);
    setLedCorr(6, 2 * i, state);
    setLedCorr(6, 2 * i + 1, state);
}

void updateLeds() {
    setRowCorr(0, 0);  // clear master clock row
    setLedCorr(0, 0, true);
    // setLedCorr(0, master_clock, true);

    for (int i = 0; i < N_CHANNELS; i++) {
        setRowCorr(i + 1, 0);  // clear active row
        for (int c = 0; c < 8; c++) {
            if (sequence[i][(c + curr_step[i]) % n_length[i]]) {
                setLedCorr(i + 1, c, true);
            }
        }

        if (mode == 0) {
            if (sequence[i][curr_step[i]] && pulses_active) {
                setChLed(i, true);  // bottom row flash
            } else {
                setChLed(i, false);
            }
        }
        // active channel indicator
        if (i == ch_active) {
            setLedCorr(7, 2 * i, true);
            setLedCorr(7, 2 * i + 1, true);
        } else {
            setLedCorr(7, 2 * i, false);
            setLedCorr(7, 2 * i + 1, false);
        }
    }
}

void updateLedsMode1() {
    setRowCorr(5, 0);
    setRowCorr(6, 0);
    for (int a = 0; a < 8; a++) {
        if (a < n_length[ch_active]) {
            setLedCorr(5, a, true);
        }
        if (a + 8 < n_length[ch_active]) {
            setLedCorr(6, a, true);
        }
    }
}

void updateLedsMode2() {
    setRowCorr(5, 0);
    setRowCorr(6, 0);
    for (int a = 0; a < 8; a++) {
        if (sequence[ch_active][a] && a < n_length[ch_active]) {
            setLedCorr(5, a, true);
        }
        if (sequence[ch_active][a + 8] && a + 8 < n_length[ch_active]) {
            setLedCorr(6, a, true);
        }
    }
}

void updateLedsMode3() {
    setRowCorr(5, 0);
    setRowCorr(6, 0);
    for (int a = 0; a < 8; a++) {
        if (a < clock_div[ch_active]) {
            setLedCorr(5, a, true);
        }
        if (a + 8 < clock_div[ch_active]) {
            setLedCorr(6, a, true);
        }
    }
}
