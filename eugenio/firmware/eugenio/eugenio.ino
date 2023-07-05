#include <EEPROM.h>
#include <LedControl.h>

/*
Eugenio - Euclidean Sequencer
Version 2.0 - 2022-11-19 Alessio Nuti
*/

#define ENC_1A 6  // n / length
#define ENC_1B 5
#define ENC_2A 8  // k / hits
#define ENC_2B 7
#define ENC_3A 10  // o / offset
#define ENC_3B 9
#define ENC_SWITCH A0
#define OUT_1 A1
#define OUT_2 A2
#define OUT_3 A3
#define OUT_4 A4
#define LED_LOAD 11
#define LED_CLK 12
#define LED_DIN 13
#define CLK_IN 2
#define RESET_IN 3
#define RESET_BUT 4
#define POT A5

#define BRIGHTNESS 15               // max brightness ok when R = 470 ohm
#define TIMEOUT_DISPLAY 4000        // ms
#define ENC_DEBOUNCE 50             // ms
#define SWITCH_DEBOUNCE 100         // ms
#define SWITCH_SHORT_PRESS_TIME 10  // ms
#define SWITCH_LONG_PRESS_TIME 500  // ms

// constants
#define N_CHANNELS 4             // number of channels
#define MAX_LENGTH 16            // max sequence length
#define MIN_LENGTH 1             // min sequence length
#define MAX_PULSE_DURATION 1000  // ms
#define MIN_PULSE_DURATION 10    // ms
#define CURVE_PULSE_DURATION 1
#define MAX_BPM 480
#define MIN_BPM 60
#define CURVE_BPM 1

LedControl lc = LedControl(LED_DIN, LED_CLK, LED_LOAD, 1);
const byte OUT_PINS[N_CHANNELS] = { OUT_1, OUT_2, OUT_3, OUT_4 };
const int SWITCH_THRESHOLD[4] = { 550, 350, 205, 120 };
bool diga_old;  // for encoders
bool digb_old;
int enc_reading[3] = { 0, 0, 0 };
float pot_val;
float pot_val_old;
int sw_val;
unsigned long time = 0;
unsigned long last_clock = 0;
unsigned long last_touched = 0;
unsigned long last_enc = 0;
unsigned long last_sw_pressed[3] = { 0, 0, 0 };
unsigned long last_sw_released[3] = { 0, 0, 0 };
bool sw_pressed[3] = { false, false, false };
bool sw_holded[3] = { false, false, false };
bool sw_short_press[3] = { false, false, false };
bool sw_long_press[3] = { false, false, false };
bool sleep_status = true;
bool sleep_command = false;
volatile bool sync_int = false;
volatile bool out_gate = false;
volatile bool reset = false;
volatile bool clock = false;
volatile bool sync = false;
int bpm_clock_int;
float divider = 0.5f;
float interval = 1000.0f;
int ch_edit = 0;  // current channel, zero indexed
int mode = 0;     // display mode
// 0 = normal
// 1 = length
// 2 = hits & offset
// 3 = gate length
// 4 = int clock tempo
// 5 = clock divider/mult
bool sequence_saved = true;
bool bpm_saved = true;
bool div_saved = true;
bool pulse_duration_saved = true;
byte master_clock = 0;
byte n_length[N_CHANNELS];   // length of the sequence
byte k_hits[N_CHANNELS];     // number of hits in the sequence
byte o_offset[N_CHANNELS];   // off-set of the sequence
byte curr_step[N_CHANNELS];  // current step
byte disp_index[N_CHANNELS];
bool sequence[N_CHANNELS][MAX_LENGTH];
bool pulse_active[N_CHANNELS];
int pulse_duration[N_CHANNELS];
unsigned long last_pulse[N_CHANNELS];
unsigned long last_sync = 0;
unsigned long lastlast_sync = 0;

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

  if (EEPROM.read(255) != 127 || EEPROM.read(256) != 128) {
    // if EEPROM is blank / corrupted, write some startup amounts
    EEPROM.write(0, 8);     // 1n
    EEPROM.write(1, 4);     // 1k
    EEPROM.write(2, 0);     // 1o
    writeIntEEPROM(3, 10);  // pulse length

    EEPROM.write(10, 8);     // 2n
    EEPROM.write(11, 4);     // 2k
    EEPROM.write(12, 0);     // 2o
    writeIntEEPROM(13, 10);  // pulse length

    EEPROM.write(20, 8);     // 3n
    EEPROM.write(21, 4);     // 3k
    EEPROM.write(22, 0);     // 3o
    writeIntEEPROM(23, 10);  // pulse length

    EEPROM.write(30, 8);     // 4n
    EEPROM.write(31, 4);     // 4k
    EEPROM.write(32, 0);     // 4o
    writeIntEEPROM(33, 10);  // pulse length

    EEPROM.write(127, 8);     // div_int
    writeIntEEPROM(128, 80);  // bpm

    EEPROM.write(255, 127);  // two values to check if EEPROM has been initialized correctly
    EEPROM.write(256, 128);
  }

  for (int i = 0; i < N_CHANNELS; i++) {
    n_length[i] = EEPROM.read(i * 10);
    k_hits[i] = EEPROM.read(i * 10 + 1);
    o_offset[i] = EEPROM.read(i * 10 + 2);
    curr_step[i] = 0;
    disp_index[i] = 0;
    pulse_active[i] = false;
    pulse_duration[i] = readIntEEPROM(i * 10 + 3);
    last_pulse[i] = 0;
  }
  updateSequence();
  divider = (((float)EEPROM.read(127)) + 0.5f) / 16.0f;
  bpm_clock_int = readIntEEPROM(128);

  // set up Arduino interrupts
  attachInterrupt(digitalPinToInterrupt(CLK_IN), clock_isr, FALLING);    // rising edge on input (input is inverted by the analog circuit)
  attachInterrupt(digitalPinToInterrupt(RESET_IN), reset_isr, FALLING);  // ""

  // The MAX72XX is in power-saving mode on startup, we have to do a wakeup call
  lc.shutdown(0, false);
  lc.setIntensity(0, BRIGHTNESS);
  lc.clearDisplay(0);
  sleep_status = false;
  startupanim();
  wakeanim();
}

void loop() {
  time = millis();

  // SLEEP ROUTINE
  if (!sleep_status && sleep_command) {
    sleepanim();
    lc.shutdown(0, true);
    sleep_status = true;
  }
  if (sleep_status && !sleep_command) {
    lc.clearDisplay(0);
    lc.shutdown(0, false);
    wakeanim();
    sleep_status = false;
  }

  // TIMEOUTS
  if (time - last_touched > TIMEOUT_DISPLAY) {
    mode = 0;
    setRowCorr(5, 0);
    setRowCorr(6, 0);
  }

  // SAVE DATA TO EEPROM

  if (mode != 1 && mode != 2 && !sequence_saved) {
    for (int i = 0; i < N_CHANNELS; i++) {
      EEPROM.write(ch_edit * 10, n_length[ch_edit]);
      EEPROM.write(ch_edit * 10 + 1, k_hits[ch_edit]);
      EEPROM.write(ch_edit * 10 + 2, o_offset[ch_edit]);
    }
    sequence_saved = true;
  }

  if (mode != 3 && !pulse_duration_saved) {
    for (int i = 0; i < N_CHANNELS; i++) {
      writeIntEEPROM(i * 10 + 3, pulse_duration[i]);
    }
    pulse_duration_saved = true;
  }
  if (mode != 4 && !bpm_saved) {
    writeIntEEPROM(128, bpm_clock_int);
    bpm_saved = true;
  }
  if (mode != 5 && !div_saved) {
    EEPROM.write(127, (byte)(divider * 16 + 0.5f));
    div_saved = true;
  }

  // READ ENCODERS
  // ENCODER 1 (N)
  enc_reading[0] = encoderRead(ENC_1A, ENC_1B);
  if (enc_reading[0] != 0 && time - last_enc > ENC_DEBOUNCE) {
    n_length[ch_edit] = constrain((int)n_length[ch_edit] + enc_reading[0], MIN_LENGTH, MAX_LENGTH);
    k_hits[ch_edit] = constrain((int)k_hits[ch_edit], 0, n_length[ch_edit]);
    sequence_saved = false;
    updateSequence();
    mode = 1;
    updateLedsMode1();
    last_enc = millis();
    last_touched = last_enc;
  }
  // ENCODER 2 (K)
  enc_reading[1] = encoderRead(ENC_2A, ENC_2B);
  if (enc_reading[1] != 0 && time - last_enc > ENC_DEBOUNCE) {
    k_hits[ch_edit] = constrain((int)k_hits[ch_edit] + enc_reading[1], 0, n_length[ch_edit]);  // update with encoder reading
    sequence_saved = false;
    updateSequence();
    mode = 2;
    updateLedsMode2();
    last_enc = millis();
    last_touched = last_enc;
  }
  // ENCODER 3 (O)
  enc_reading[2] = encoderRead(ENC_3A, ENC_3B);
  if (enc_reading[2] != 0 && time - last_enc > ENC_DEBOUNCE) {
    o_offset[ch_edit] += (enc_reading[2] + n_length[ch_edit]);
    o_offset[ch_edit] %= n_length[ch_edit];  // update with encoder reading
    sequence_saved = false;
    updateSequence();
    mode = 2;
    updateLedsMode2();
    last_enc = millis();
    last_touched = last_enc;
  }

  // READ SWITCHES
  sw_val = analogRead(ENC_SWITCH);
  // SWITCH 1
  switchRead(0, sw_val);
  if (sw_short_press[0]) {
    if (sync_int) {
      // TEMPO
      sw_short_press[0] = false;
      mode = 4;
      updateLedsMode34(potNormalizeParam(bpm_clock_int, MIN_BPM, MAX_BPM, CURVE_BPM));
    } else {
      // CLOCK DIV/MULT
      sw_short_press[0] = false;
      mode = 5;
      updateLedsMode5(divider);
    }
  }
  if (sw_long_press[0]) {
    sw_long_press[0] = false;
    // SYNC MODE
    sync_int = !sync_int;
    mode = 0;
  }
  // SWITCH 2
  switchRead(1, sw_val);
  if (sw_short_press[1]) {
    sw_short_press[1] = false;
    // CHANNEL EDIT
    ch_edit = (ch_edit + 1) % N_CHANNELS;
    switch (mode) {
      case 1:
        updateLedsMode1();
        break;
      case 2:
        updateLedsMode2();
        break;
      case 3:
        updateLedsMode34(potNormalizeParam(pulse_duration[ch_edit], MIN_PULSE_DURATION, MAX_PULSE_DURATION, CURVE_PULSE_DURATION));
        break;
    }
  }
  if (sw_long_press[1]) {
    sw_long_press[1] = false;
    // SLEEP
    sleep_command = !sleep_command;
  }
  // SWITCH 3
  switchRead(2, sw_val);
  if (sw_short_press[2] && out_gate) {
    sw_short_press[2] = false;
    // GATE LENGTH
    mode = 3;
    updateLedsMode34(potNormalizeParam(pulse_duration[ch_edit], MIN_PULSE_DURATION, MAX_PULSE_DURATION, CURVE_PULSE_DURATION));
  }
  if (sw_long_press[2]) {
    sw_long_press[2] = false;
    // GATE/TRG MODE
    out_gate = !out_gate;
  }

  // READ POTENTIOMETER
  pot_val = 1.0f - (analogRead(POT) / 1023.0f);
  float delta = pot_val - pot_val_old;
  if (fabs(delta) > 0.005f) {  // pot touched
    last_touched = time;
    if (mode == 3) {
      pulse_duration_saved = false;
      float param = potNewParam(delta, potNormalizeParam(pulse_duration[ch_edit], MIN_PULSE_DURATION, MAX_PULSE_DURATION, CURVE_PULSE_DURATION), pot_val_old);
      pulse_duration[ch_edit] = potCalculateParam(param, MIN_PULSE_DURATION, MAX_PULSE_DURATION, CURVE_PULSE_DURATION);
      updateLedsMode34(param);
    } else if (mode == 4) {
      bpm_saved = false;
      float param = potNewParam(delta, potNormalizeParam(bpm_clock_int, MIN_BPM, MAX_BPM, CURVE_BPM), pot_val_old);
      bpm_clock_int = potCalculateParam(param, MIN_BPM, MAX_BPM, CURVE_BPM);
      updateLedsMode34(param);
    } else if (mode == 5) {
      div_saved = false;
      divider = potNewParam(delta, divider, pot_val_old);
      updateLedsMode5(divider);
    }
    pot_val_old = pot_val;
  }

  // SYNC MANAGEMENT
  if (sync_int) {  // INTERNAL CLOCK
    if (time - last_clock > 60.0f / bpm_clock_int * 1000) {
      clock = true;
    }
  } else {  // EXTERNAL CLOCK
    if (sync) {
      sync = false;
      interval = ((last_sync - lastlast_sync) + (time - last_sync)) * 0.5f;
      lastlast_sync = last_sync;
      last_sync = time;
      if (divCalculate(divider) == 1) clock = true;
    }
    if (divCalculate(divider) != 1 && last_sync != 0 && time - last_clock > interval * divCalculate(divider)) {
      // CLOCK DIV/MULT
      clock = true;
    }
  }

  // CLOCK ADVANCE ROUTINE
  if (clock) {
    clock = false;
    if (reset || digitalRead(RESET_BUT) == HIGH) {
      reset = false;
      master_clock = 0;
      for (int i = 0; i < N_CHANNELS; i++) {
        curr_step[i] = 0;
        disp_index[i] = 0;
      }
    } else {
      master_clock = (master_clock + 1) % 8;
      for (int i = 0; i < N_CHANNELS; i++) {
        // update current step and display index
        curr_step[i] = (curr_step[i] + 1) % n_length[i];
        if (master_clock == 0) {
          disp_index[i] = (disp_index[i] + 8) % n_length[i];
        }
      }
    }
    for (int i = 0; i < N_CHANNELS; i++) {
      // set outputs
      if (sequence[i][curr_step[i]]) {
        digitalWrite(OUT_PINS[i], HIGH);
        pulse_active[i] = true;
        last_pulse[i] = time;
      }
    }
    last_clock = time;
  }

  // FINISH PULSES
  for (int i = 0; i < N_CHANNELS; i++) {
    if (((!out_gate && time - last_pulse[i] > MIN_PULSE_DURATION) || (out_gate && time - last_pulse[i] > pulse_duration[i])) && pulse_active[i] == true) {
      digitalWrite(OUT_PINS[i], LOW);
      pulse_active[i] = false;
    }
  }

  updateLeds();
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

void switchRead(int n, int sw_value) {
  if (sw_value > SWITCH_THRESHOLD[n + 1] && sw_value < SWITCH_THRESHOLD[n] && time - last_sw_pressed[n] > SWITCH_DEBOUNCE && time - last_sw_released[n] > SWITCH_DEBOUNCE) {
    if (!sw_pressed[n]) {
      if (time - last_sw_pressed[n] > SWITCH_SHORT_PRESS_TIME) {
        sw_pressed[n] = true;
        last_sw_pressed[n] = millis();
        last_touched = last_sw_pressed[n];
      }
    } else if (!sw_holded[n]) {
      if (time - last_sw_pressed[n] >= SWITCH_LONG_PRESS_TIME) {
        sw_holded[n] = true;
        sw_long_press[n] = true;
      }
    }
  } else if (sw_pressed[n] && time - last_sw_pressed[n] > SWITCH_DEBOUNCE && time - last_sw_released[n] > SWITCH_DEBOUNCE) {
    sw_pressed[n] = false;
    sw_holded[n] = false;
    last_sw_released[n] = millis();
    last_touched = last_sw_released[n];
    if (last_sw_released[n] - last_sw_pressed[n] < SWITCH_LONG_PRESS_TIME) {
      sw_short_press[n] = true;
    }
  }
}

void startupanim() {
  int delay1 = 80;
  int delay2 = 40;
  setRowCorr(0, B00000000);
  setRowCorr(1, B00001100);
  setRowCorr(2, B00010100);
  setRowCorr(3, B00100100);
  setRowCorr(4, B00100100);
  setRowCorr(5, B00010100);
  setRowCorr(6, B00001100);
  setRowCorr(7, B00000000);
  delay(delay1);
  lc.clearDisplay(0);
  delay(delay2);
  setRowCorr(0, B00000000);
  setRowCorr(1, B00010000);
  setRowCorr(2, B00001000);
  setRowCorr(3, B00001100);
  setRowCorr(4, B00110000);
  setRowCorr(5, B00010000);
  setRowCorr(6, B00001000);
  setRowCorr(7, B00000000);
  delay(delay1);
  lc.clearDisplay(0);
  delay(delay2);
  setRowCorr(0, B00000000);
  setRowCorr(1, B00011100);
  setRowCorr(2, B00100100);
  setRowCorr(3, B00010100);
  setRowCorr(4, B00010100);
  setRowCorr(5, B00100100);
  setRowCorr(6, B00011100);
  setRowCorr(7, B00000000);
  delay(delay1);
  lc.clearDisplay(0);
  delay(delay2);
  setRowCorr(0, B00000000);
  setRowCorr(1, B00011100);
  setRowCorr(2, B00100100);
  setRowCorr(3, B00010100);
  setRowCorr(4, B00001100);
  setRowCorr(5, B00010100);
  setRowCorr(6, B00100100);
  setRowCorr(7, B00000000);
  delay(delay1);
  lc.clearDisplay(0);
  delay(delay2);
  setRowCorr(0, B00000000);
  setRowCorr(1, B00011100);
  setRowCorr(2, B00100100);
  setRowCorr(3, B00010100);
  setRowCorr(4, B00001100);
  setRowCorr(5, B00010100);
  setRowCorr(6, B00100100);
  setRowCorr(7, B00000000);
  delay(delay1);
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
  if (!sync_int)
    sync = true;
}

void reset_isr() {
  reset = true;
}

void setLedCorr(int row, int col, bool state) {
  lc.setLed(0, 7 - row, 7 - col, state);
}

void setRowCorr(int row, byte val) {
  lc.setRow(0, 7 - row, val);
}

void updateLeds() {
  setRowCorr(0, 0);  // clear master clock row
  setLedCorr(0, master_clock, true);
  for (int i = 0; i < N_CHANNELS; i++) {
    setRowCorr(i + 1, 0);  // clear active row
    for (int c = 0; c < 8; c++) {
      if (sequence[i][(c + disp_index[i]) % n_length[i]]) {
        setLedCorr(i + 1, c, true);
      }
    }
    // bottom row flash
    if (mode == 0 || mode == 3 || mode == 4) {
      if (sequence[i][curr_step[i]] && pulse_active[i]) {
        setLedCorr(6, 2 + i, true);
      } else {
        setLedCorr(6, 2 + i, false);
      }
    }
    // current channel indicator
    if (i == ch_edit) {
      setLedCorr(7, 2 + i, true);
    } else {
      setLedCorr(7, 2 + i, false);
    }
  }  // sync mode indicator
  if (sync_int) {
    setLedCorr(7, 0, false);
    setLedCorr(7, 1, true);
  } else {
    setLedCorr(7, 0, true);
    setLedCorr(7, 1, false);
  }
  // out mode indicator
  if (out_gate) {
    setLedCorr(7, 6, true);
    setLedCorr(7, 7, false);
  } else {
    setLedCorr(7, 6, false);
    setLedCorr(7, 7, true);
  }
}

void updateLedsMode1() {
  setRowCorr(5, 0);
  setRowCorr(6, 0);
  for (int a = 0; a < 8; a++) {
    if (a < n_length[ch_edit]) {
      setLedCorr(5, a, true);
    }
    if (a + 8 < n_length[ch_edit]) {
      setLedCorr(6, a, true);
    }
  }
}

void updateLedsMode2() {
  setRowCorr(5, 0);
  setRowCorr(6, 0);
  for (int a = 0; a < 8; a++) {
    if (sequence[ch_edit][a] && a < n_length[ch_edit]) {
      setLedCorr(5, a, true);
    }
    if (sequence[ch_edit][a + 8] && a + 8 < n_length[ch_edit]) {
      setLedCorr(6, a, true);
    }
  }
}

void updateLedsMode34(float param_normalized) {
  setRowCorr(5, 0);
  for (int a = 0; a < 8; a++) {
    if (a < param_normalized * 7 + 0.5f)
      setLedCorr(5, a, true);
  }
}

void updateLedsMode5(float div_normalized) {
  setRowCorr(5, 0);
  setRowCorr(6, 0);
  int div_int = div_normalized * 16 + 0.5f;
  if (div_int < 7 && div_int >= 0) {
    // DIVIDER
    for (int a = 0; a < 8; a++) {
      if (a >= div_int)
        setLedCorr(5, a, true);
    }
  } else if (div_int > 8 && div_int <= 16) {
    // MULTIPLIER
    for (int a = 0; a < 8; a++) {
      if (a <= div_int - 8)
        setLedCorr(6, a, true);
    }
  } else {
    // OFF
    setLedCorr(5, 7, true);
    setLedCorr(6, 0, true);
  }
}

float potNewParam(float delta, float parameter_value, float previous_pot_value) {
  float skew_ratio = delta > 0.0f
                       ? (1.001f - parameter_value) / (1.001f - pot_val_old)
                       : (0.001f + parameter_value) / (0.001f + pot_val_old);
  skew_ratio = constrain(skew_ratio, 0.1f, 10.0f);
  float newparam = parameter_value + skew_ratio * delta;
  newparam = constrain(newparam, 0.0f, 1.0f);
  return newparam;
}

float potNormalizeParam(float param, float min_param, float max_param, float power) {
  return pow((param - min_param) / (max_param - min_param), 1.0f / power);
}

float potCalculateParam(float param_normalized, float min_param, float max_param, float power) {
  return min_param + pow(param_normalized, power) * (max_param - min_param);
}

void writeIntEEPROM(int address, int number)
{ 
  byte byte1 = number >> 8;
  byte byte2 = number & 0xFF;
  EEPROM.write(address, byte1);
  EEPROM.write(address + 1, byte2);
}

int readIntEEPROM(int address)
{
  byte byte1 = EEPROM.read(address);
  byte byte2 = EEPROM.read(address + 1);
  return (byte1 << 8) + byte2;
}

float divCalculate(float div_normalized) {
  int div_int = div_normalized * 16 + 0.5f;
  if (div_int < 7 && div_int >= 0) {
    // DIVIDER
    return (8 - div_int);
  } else if (div_int > 8 && div_int <= 16) {
    // MULTIPLIER
    return 1.0f / (div_int - 7.0f);
  } else {
    // OFF
    return 1;
  }
}