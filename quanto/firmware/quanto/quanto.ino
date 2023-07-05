#include <EEPROM.h>
#include <SPI.h>

/*
Quanto - CV Quantizer
Version 4.0 - 2023-06-17 Alessio Nuti 
based on pureQuantiser - Rob Spencer cc-by 4.0
*/

#define LR_0 A3
#define LR_1 A4
#define LR_2 A5
#define LR_3 7
#define LC_0 A0
#define LC_1 A1
#define LC_2 A2
#define BC_0 2
#define BC_1 1
#define BC_2 0
#define BR_0 3
#define BR_1 4
#define BR_2 5
#define BR_3 6
#define SW_0 A6
#define SW_1 A7
#define CS_ADC 9
#define CS_DAC 10
#define TRIG 8

#define RANGE_IN 15.0F            // total range in octaves
#define RANGE_OUT 10.0F           // total range in octaves
#define NOTE_VALUE 0.0833333333F  // 1oct/12
#define TRIG_PULSE 10             // ms

#define SWITCH_DEBOUNCE 300          // ms
#define SWITCH_SHORT_PRESS_TIME 10   // ms
#define SWITCH_LONG_PRESS_TIME 3000  // ms
#define SWITCH_THRESHOLD 300

byte mode = 0;  // 0=bypass 1=quantize 2=ADCcalibration 3=DACcalibration
unsigned long time = 0;
float ADC_MIN = 0;
float ADC_MAX = 4095;
byte octTranspose = 5;  // transpose DOWN
bool notes[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int i = 0;
float cvIn;   // from 0 to RANGE_IN
float cvOut;  // from 0 to RANGE_OUT
float lastCvOut;
unsigned long lastCvChangeTime;

// Setup switch variables
unsigned long lastDebounceTime = 0;
bool sw_0;
bool sw_1;
int adc_0v;
int adc_5v;
unsigned long last_sw_pressed[2] = { 0, 0 };
unsigned long last_sw_released[2] = { 0, 0 };
bool sw_pressed[2] = { false, false };
bool sw_holded[2] = { false, false };
bool sw_short_press[2] = { false, false };
bool sw_long_press[2] = { false, false };
bool mode_2_flag = false;

void setup() {
  pinMode(LR_0, OUTPUT);
  pinMode(LR_1, OUTPUT);
  pinMode(LR_2, OUTPUT);
  pinMode(LR_3, OUTPUT);
  pinMode(LC_0, OUTPUT);
  pinMode(LC_1, OUTPUT);
  pinMode(LC_2, OUTPUT);
  digitalWrite(LR_0, HIGH);
  digitalWrite(LR_1, HIGH);
  digitalWrite(LR_2, HIGH);
  digitalWrite(LR_3, HIGH);
  digitalWrite(LC_0, LOW);
  digitalWrite(LC_1, LOW);
  digitalWrite(LC_2, LOW);

  pinMode(BC_0, OUTPUT);
  pinMode(BC_1, OUTPUT);
  pinMode(BC_2, OUTPUT);
  pinMode(BR_0, INPUT);
  pinMode(BR_1, INPUT);
  pinMode(BR_2, INPUT);
  pinMode(BR_3, INPUT);
  pinMode(SW_0, INPUT);
  pinMode(SW_1, INPUT);
  digitalWrite(BC_0, LOW);
  digitalWrite(BC_1, LOW);
  digitalWrite(BC_2, LOW);

  pinMode(CS_ADC, OUTPUT);
  pinMode(CS_DAC, OUTPUT);
  digitalWrite(CS_ADC, HIGH);
  digitalWrite(CS_DAC, HIGH);
  SPI.begin();

  pinMode(TRIG, OUTPUT);
  digitalWrite(TRIG, LOW);

  // Read saved notes from EEPROM
  for (int k = 0; k < 12; k++) {
    if (EEPROM.read(k) != 255) {
      notes[k] = EEPROM.read(k);
    }
  }
  if (EEPROM.read(100) != 255) {
    int octTransposeStored = EEPROM.read(100);
    octTranspose = constrain(octTransposeStored, 0, 10);
  }
  if (EEPROM.read(110) != 255 && EEPROM.read(120) != 255) {
    ADC_MIN = readIntEEPROM(110);
    ADC_MAX = readIntEEPROM(120);
  }
}

void loop() {
  time = millis();

  // SWITCH 0
  switchRead(0, analogRead(SW_0));
  if (sw_short_press[0]) {
    sw_short_press[0] = false;
    if (mode == 0 || mode == 1) {
      octTranspose += 1;
      octTranspose = constrain(octTranspose, 0, 10);
      EEPROM.write(100, octTranspose);
    } else if (mode == 2) {
      if (!mode_2_flag) {
        int adc_5v = adcAvg(100);  // 5 V
        mode_2_flag = true;
      } else {
        int adc_0v = adcAvg(100);  // 0 V
        ADC_MIN = 2*adc_5v - adc_0v;
        ADC_MAX = 2*adc_0v - adc_5v;
        ADC_MIN = constrain(ADC_MIN, 0, 4095);
        ADC_MAX = constrain(ADC_MAX, 0, 4095);
        // store adc calibration
        writeIntEEPROM(110, ADC_MIN);
        writeIntEEPROM(120, ADC_MAX);
        // exit mode 2
        mode_2_flag = false;
        mode = 0;
      }
    }
  }
  if (sw_long_press[0]) {
    sw_long_press[0] = false;
    if (mode == 2) mode = 0;
    else mode = 2;
  }
  // SWITCH 1
  switchRead(1, analogRead(SW_1));
  if (sw_short_press[1]) {
    sw_short_press[1] = false;
    if (mode == 0 || mode == 1) {
      if (octTranspose > 0) octTranspose -= 1;
      octTranspose = constrain(octTranspose, 0, 10);
      EEPROM.write(100, octTranspose);
    } else if (mode == 3) mode = 0;
  }
  if (sw_long_press[1]) {
    sw_long_press[1] = false;
    if (mode == 3) mode = 0;
    else mode = 3;
  }

  if (mode == 0 || mode == 1) {
    // Check all the note flags and if none are set, we're not quantising, mode=0
    int notes_active = 0;
    for (int j = 0; j < 12; j++) {
      notes_active = (notes_active + (int)notes[j]);
    }
    if (notes_active == 0) mode = 0;
    else {
      mode = 1;
      if (notes[i]) writeLED(i);
      else writeLED(-1);
    }
  }

  // 2) Button Management
  readNoteButton(i);
  // Increment the counter so on the next loop we'll read the next button.
  if (i > 11) {
    i = 0;
  } else {
    i++;
  }

  // 3) CV Management
  // Read CV In
  cvIn = adc2Cv(adcAvg(10));
  if (mode == 1) {
    // Quantize
    cvOut = quantizeCV(cvIn);
    // Transpose and clamp output
    cvOut = constrain(cvOut - octTranspose, 0, RANGE_OUT);
    // CV Out has changed, i.e. we've changed notes, then set the trigger out high.
    if (abs(lastCvOut - cvOut) > NOTE_VALUE / 2) {
      digitalWrite(TRIG, HIGH);
      lastCvOut = cvOut;
      lastCvChangeTime = millis();
    } else if ((millis() - lastCvChangeTime) > TRIG_PULSE) {
      digitalWrite(TRIG, LOW);
    }
  } else if (mode == 2) {
    digitalWrite(TRIG, LOW);
    // Transpose and clamp output
    cvOut = constrain(cvIn - 5, 0, RANGE_OUT);
    if (mode_2_flag) blinkLed(11);
    else blinkLed(0);
  } else if (mode == 3) {
    digitalWrite(TRIG, LOW);
    if (notes[0]) {
      cvOut = RANGE_OUT;
      writeLED(0);
    } else if (notes[5]) {
      cvOut = RANGE_OUT / 2;
      writeLED(5);
    } else {
      cvOut = 0;
      writeLED(11);
    }
  } else {
    digitalWrite(TRIG, LOW);
    // Light the leds like a VU Meter.
    vuMeter(cvIn);
    // Transpose and clamp output
    cvOut = constrain(cvIn - octTranspose, 0, RANGE_OUT);
  }
  // Write to DAC
  dacWrite(cv2Dac(cvOut));
}

float quantizeCV(float cvInput) {
  int octave = floor(cvInput);
  float noteFloat = (cvInput - octave) / NOTE_VALUE;
  int note = round(noteFloat);  // 0 to 11
  int sign = 1;
  if (noteFloat < note) sign = -1;
  // Quantize
  float cvOutput = 0;
  for (int m = 0; m < 12; m++) {
    cvOutput = octave + (note + sign * m) * NOTE_VALUE;
    if (cvOutput >= 0 && cvOutput <= 4095 && notes[(note + sign * m + 12) % 12]) {
      break;
    }
    cvOutput = octave + (note - sign * m) * NOTE_VALUE;
    if (cvOutput >= 0 && cvOutput <= 4095 && notes[(note - sign * m + 12) % 12]) {
      break;
    }
  }
  return cvOutput;
}

void vuMeter(int cv) {
  for (int l = 0; l < 12; l++) {
    if (cv > (RANGE_IN / 13 * (l + 1))) {
      writeLED(11 - l);
      writeLED(-1);
    } 
  }
}

void dacWrite(int value) {
  digitalWrite(CS_DAC, LOW);
  byte data = value >> 8;
  data = data & B00001111;
  data = data | B00110000;
  SPI.transfer(data);
  data = value;
  SPI.transfer(data);
  digitalWrite(CS_DAC, HIGH);
}

int adcRead(byte channel) {
  byte commandbits = B00001101;  //command bits - 0000, start, mode, chn, MSBF
  unsigned int b1 = 0;           // get the return var's ready
  unsigned int b2 = 0;
  commandbits |= (channel << 1);  // update the command bit to select either ch 1 or 2
  digitalWrite(CS_ADC, LOW);
  SPI.transfer(commandbits);        // send out the command bits
  const int hi = SPI.transfer(b1);  // read back the result high byte
  const int lo = SPI.transfer(b2);  // then the low byte
  digitalWrite(CS_ADC, HIGH);
  b1 = lo + (hi << 8);  // assemble the two bytes into a word
  return (b1 >> 3);     // To have a 12bit answer (see datasheet)
}

int adcAvg(int points) {
  long adc_sum = 0;
  for (int k = 0; k < points; k++) adc_sum += adcRead(0);
  return adc_sum / points;
}

void switchRead(int n, int sw_value) {
  if (sw_value > SWITCH_THRESHOLD && time - last_sw_pressed[n] > SWITCH_DEBOUNCE && time - last_sw_released[n] > SWITCH_DEBOUNCE) {
    if (!sw_pressed[n]) {
      if (time - last_sw_pressed[n] > SWITCH_SHORT_PRESS_TIME) {
        sw_pressed[n] = true;
        last_sw_pressed[n] = millis();
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
    if (last_sw_released[n] - last_sw_pressed[n] < SWITCH_LONG_PRESS_TIME) {
      sw_short_press[n] = true;
    }
  }
}

float adc2Cv(int x) {
  // map ADC counts (ADC_MIN-ADC_MAX) to cv (RANGE_IN-0)
  float y = constrain(x, ADC_MIN, ADC_MAX);
  return (ADC_MAX - y) * RANGE_IN / (ADC_MAX - ADC_MIN);
}

int cv2Dac(float x) {
  // map cv (0-RANGE_OUT) to DAC counts (0-4095)
  return round(x * 4095.0F / RANGE_OUT);
}

void writeIntEEPROM(int address, int number) {
  byte byte1 = number >> 8;
  byte byte2 = number & 0xFF;
  EEPROM.write(address, byte1);
  EEPROM.write(address + 1, byte2);
}

int readIntEEPROM(int address) {
  byte byte1 = EEPROM.read(address);
  byte byte2 = EEPROM.read(address + 1);
  return (byte1 << 8) + byte2;
}

void readNoteButton(int button) {
  int buttonState;
  switch (button) {
    case 0:
      digitalWrite(BC_2, HIGH);
      buttonState = digitalRead(BR_0);
      digitalWrite(BC_2, LOW);
      break;
    case 1:
      digitalWrite(BC_2, HIGH);
      buttonState = digitalRead(BR_1);
      digitalWrite(BC_2, LOW);
      break;
    case 2:
      digitalWrite(BC_2, HIGH);
      buttonState = digitalRead(BR_2);
      digitalWrite(BC_2, LOW);
      break;
    case 3:
      digitalWrite(BC_2, HIGH);
      buttonState = digitalRead(BR_3);
      digitalWrite(BC_2, LOW);
      break;
    case 4:
      digitalWrite(BC_1, HIGH);
      buttonState = digitalRead(BR_0);
      digitalWrite(BC_1, LOW);
      break;
    case 5:
      digitalWrite(BC_1, HIGH);
      buttonState = digitalRead(BR_1);
      digitalWrite(BC_1, LOW);
      break;
    case 6:
      digitalWrite(BC_1, HIGH);
      buttonState = digitalRead(BR_2);
      digitalWrite(BC_1, LOW);
      break;
    case 7:
      digitalWrite(BC_1, HIGH);
      buttonState = digitalRead(BR_3);
      digitalWrite(BC_1, LOW);
      break;
    case 8:
      digitalWrite(BC_0, HIGH);
      buttonState = digitalRead(BR_0);
      digitalWrite(BC_0, LOW);
      break;
    case 9:
      digitalWrite(BC_0, HIGH);
      buttonState = digitalRead(BR_1);
      digitalWrite(BC_0, LOW);
      break;
    case 10:
      digitalWrite(BC_0, HIGH);
      buttonState = digitalRead(BR_2);
      digitalWrite(BC_0, LOW);
      break;
    case 11:
      digitalWrite(BC_0, HIGH);
      buttonState = digitalRead(BR_3);
      digitalWrite(BC_0, LOW);
      break;
  }

  // Debounce and toggle
  if ((time - lastDebounceTime) > SWITCH_DEBOUNCE) {
    if (buttonState == HIGH) {
      notes[button] = !notes[button];
      EEPROM.write(button, notes[button]);
      lastDebounceTime = time;
    }
  }
}

void writeLED(int led) {
  switch (led) {
    case -1:  // no LEDs
      digitalWrite(LR_0, HIGH);
      digitalWrite(LR_1, HIGH);
      digitalWrite(LR_2, HIGH);
      digitalWrite(LR_3, HIGH);
      digitalWrite(LC_0, LOW);
      digitalWrite(LC_1, LOW);
      digitalWrite(LC_2, LOW);
      break;
    case 0:
      digitalWrite(LR_0, LOW);
      digitalWrite(LR_1, HIGH);
      digitalWrite(LR_2, HIGH);
      digitalWrite(LR_3, HIGH);
      digitalWrite(LC_0, LOW);
      digitalWrite(LC_1, LOW);
      digitalWrite(LC_2, HIGH);
      break;
    case 1:
      digitalWrite(LR_0, HIGH);
      digitalWrite(LR_1, LOW);
      digitalWrite(LR_2, HIGH);
      digitalWrite(LR_3, HIGH);
      digitalWrite(LC_0, LOW);
      digitalWrite(LC_1, LOW);
      digitalWrite(LC_2, HIGH);
      break;
    case 2:
      digitalWrite(LR_0, HIGH);
      digitalWrite(LR_1, HIGH);
      digitalWrite(LR_2, LOW);
      digitalWrite(LR_3, HIGH);
      digitalWrite(LC_0, LOW);
      digitalWrite(LC_1, LOW);
      digitalWrite(LC_2, HIGH);
      break;
    case 3:
      digitalWrite(LR_0, HIGH);
      digitalWrite(LR_1, HIGH);
      digitalWrite(LR_2, HIGH);
      digitalWrite(LR_3, LOW);
      digitalWrite(LC_0, LOW);
      digitalWrite(LC_1, LOW);
      digitalWrite(LC_2, HIGH);
      break;
    case 4:
      digitalWrite(LR_0, LOW);
      digitalWrite(LR_1, HIGH);
      digitalWrite(LR_2, HIGH);
      digitalWrite(LR_3, HIGH);
      digitalWrite(LC_0, LOW);
      digitalWrite(LC_1, HIGH);
      digitalWrite(LC_2, LOW);
      break;
    case 5:
      digitalWrite(LR_0, HIGH);
      digitalWrite(LR_1, LOW);
      digitalWrite(LR_2, HIGH);
      digitalWrite(LR_3, HIGH);
      digitalWrite(LC_0, LOW);
      digitalWrite(LC_1, HIGH);
      digitalWrite(LC_2, LOW);
      break;
    case 6:
      digitalWrite(LR_0, HIGH);
      digitalWrite(LR_1, HIGH);
      digitalWrite(LR_2, LOW);
      digitalWrite(LR_3, HIGH);
      digitalWrite(LC_0, LOW);
      digitalWrite(LC_1, HIGH);
      digitalWrite(LC_2, LOW);
      break;
    case 7:
      digitalWrite(LR_0, HIGH);
      digitalWrite(LR_1, HIGH);
      digitalWrite(LR_2, HIGH);
      digitalWrite(LR_3, LOW);
      digitalWrite(LC_0, LOW);
      digitalWrite(LC_1, HIGH);
      digitalWrite(LC_2, LOW);
      break;
    case 8:
      digitalWrite(LR_0, LOW);
      digitalWrite(LR_1, HIGH);
      digitalWrite(LR_2, HIGH);
      digitalWrite(LR_3, HIGH);
      digitalWrite(LC_0, HIGH);
      digitalWrite(LC_1, LOW);
      digitalWrite(LC_2, LOW);
      break;
    case 9:
      digitalWrite(LR_0, HIGH);
      digitalWrite(LR_1, LOW);
      digitalWrite(LR_2, HIGH);
      digitalWrite(LR_3, HIGH);
      digitalWrite(LC_0, HIGH);
      digitalWrite(LC_1, LOW);
      digitalWrite(LC_2, LOW);
      break;
    case 10:
      digitalWrite(LR_0, HIGH);
      digitalWrite(LR_1, HIGH);
      digitalWrite(LR_2, LOW);
      digitalWrite(LR_3, HIGH);
      digitalWrite(LC_0, HIGH);
      digitalWrite(LC_1, LOW);
      digitalWrite(LC_2, LOW);
      break;
    case 11:
      digitalWrite(LR_0, HIGH);
      digitalWrite(LR_1, HIGH);
      digitalWrite(LR_2, HIGH);
      digitalWrite(LR_3, LOW);
      digitalWrite(LC_0, HIGH);
      digitalWrite(LC_1, LOW);
      digitalWrite(LC_2, LOW);
      break;
  }
}

void blinkLed(int led) {
  if ((time / 100) % 2 == 0) writeLED(led);
  else writeLED(-1);
}
