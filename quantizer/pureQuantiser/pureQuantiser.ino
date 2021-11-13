/*

GMSN Pure Quantiser
Rob Spencer cc-by 4.0

Version 2021-11-12 Alessio Nuti
Release Notes:

*/

#include <EEPROM.h>

#include "SPI.h"

//Global parameters
const byte RANGE = 5;          // total range in octaves
const boolean BIPOLAR = true;  // if true input and output cv is from -RANGE/2 to +RANGE/2 volts
                               // if false input and output cv is from 0 to +RANGE volts
const float ADC_FS = 4000;
const float octVal = ADC_FS / RANGE;
const float noteVal = octVal / 12.0;

//Setup LED Pin Variables
const byte LR_0 = A3;
const byte LR_1 = A4;
const byte LR_2 = A5;
const byte LR_3 = 7;
const byte LC_0 = A0;
const byte LC_1 = A1;
const byte LC_2 = A2;

//Setup Button Pin Variables
const byte BC_0 = 2;
const byte BC_1 = 1;
const byte BC_2 = 0;
const byte BR_0 = 3;
const byte BR_1 = 4;
const byte BR_2 = 5;
const byte BR_3 = 6;

//Setup variable for the notes
int notes[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

//Setup program control variables
int mode = 0;
int i = 0;

//Setup switch debounce variables
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 500;

//Setup SPI Bus Pin Variables
const byte CS_ADC = 9;
const byte CS_DAC = 10;

//Setup CV and Trigger Pin Variables
const byte TRIG = 8;
int cvIn;
int cvOut;
int lastCvOut;
unsigned long lastCvChangeTime;
int triggerOnTime = 10;

void setup() {
    //Setup LED Pins
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

    //Setup Button Pins
    pinMode(BC_0, OUTPUT);
    pinMode(BC_1, OUTPUT);
    pinMode(BC_2, OUTPUT);
    pinMode(BR_0, INPUT);
    pinMode(BR_1, INPUT);
    pinMode(BR_2, INPUT);
    pinMode(BR_3, INPUT);

    digitalWrite(BC_0, LOW);
    digitalWrite(BC_1, LOW);
    digitalWrite(BC_2, LOW);

    //Setup SPI Chip Select Pins
    pinMode(CS_ADC, OUTPUT);
    pinMode(CS_DAC, OUTPUT);

    digitalWrite(CS_ADC, HIGH);
    digitalWrite(CS_DAC, HIGH);
    SPI.begin();

    //Setup Trigger Pin
    pinMode(TRIG, OUTPUT);
    digitalWrite(TRIG, LOW);

    //Read last saved notes from EEPROM
    for (int k = 0; k < 12; k++) {
        if (EEPROM[k] == 255) {
            notes[k] = 0;
        } else {
            notes[k] = EEPROM[k];
        }
    }
}

void loop() {
    //The main loop() is made of two part.
    //1) Button Management. Each loop through reads and lights one of the 12 buttons.
    //2) CV Management. Each loop reads the CV In, then depending on the mode, will either Quantiser the CV and look after the Trigger, or will pass the CV straight through with no trigger.

    //1) Button Management

    //Read Buttons to see if they are being pressed in this cycle.
    readNoteButton(i);

    //Light the LED if the note is active
    if (notes[i] == 1) {
        writeLED(i);
    } else {
        writeLED(-1);
    }

    //Increment the counter so on the next loop we'll read the next button.
    if (i > 11) {
        i = 0;
    } else {
        i++;
    }

    //2) CV Management

    //Read CV In
    cvIn = adcRead(0);

    //Check all the note flags and if none are set, we're not quantising, mode=0
    mode = 0;
    for (int j = 0; j < 12; j++) {
        mode = mode + notes[j];
    }

    //If mode=0, just output the input and flash the leds like a VU Meter.
    if (mode == 0) {
        cvOut = cvIn;
        vuMeter(cvOut);
    } else {
        //If not in mode=0, Quantise CV
        cvOut = quantiseCV(cvIn);

        //CV Out has changed, i.e. we've changed notes, then set the trigger out high.
        if (lastCvOut != cvOut) {
            digitalWrite(TRIG, HIGH);
            lastCvOut = cvOut;
            lastCvChangeTime = millis();
        } else if ((millis() - lastCvChangeTime) > triggerOnTime) {
            digitalWrite(TRIG, LOW);
        }
    }
    dacWrite(cvOut);
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

    //Debounce and toggle
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (buttonState == HIGH) {
            notes[button] = !notes[button];
            EEPROM[button] = notes[button];
            lastDebounceTime = millis();
        }
    }
}

void writeLED(int LED) {
    switch (LED) {
        case -1:  // NO LEDS
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
    delay(1);
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

int quantiseCV(int cvInput) {
    int octave = floor(cvInput / octVal);
    float noteFloat = (cvInput / octVal - octave) / noteVal;
    int note = round(noteFloat);  // valore da 0 a 11

    int sign = 1;
    if (noteFloat < note) {
        sign = -1;
    }

    float cvOutput = 0;
    for (int n = 0; n < 12; n++) {
        if (notes[(note + sign * n) % 11]) {
            cvOutput = octave * octVal + (note + n) * noteVal;
            break;
        }
        if (notes[(note - sign * n) % 11]) {
            cvOutput = octave * octVal + (note - n) * noteVal;
            break;
        }
    }
    return round(cvOutput);

    //     float vOffset_sup = 0;
    //     int index_sup = 0;
    //     float vOffset_inf = 0;
    //     int index_inf = 0;
    //     for (int index = 1; index < 13; index++) {
    //         if (note + index < 12) {
    //             if (notes[note + index]) {
    //                 vOffset_sup = (note + index) * noteVal;
    //                 index_sup = index;
    //                 break;
    //             }
    //         } else {
    //             if (notes[note + index - 12]) {
    //                 vOffset_sup = (note + index) * noteVal;
    //                 index_sup = index;
    //                 break;
    //             }
    //         }
    //     }
    //     for (int index = 1; index < 13; index++) {
    //         if (note - index >= 0) {
    //             if (notes[note - index]) {
    //                 vOffset_inf = (note - index) * noteVal;
    //                 index_inf = index;
    //                 break;
    //             }
    //         } else {
    //             if (notes[note - index + 12]) {
    //                 vOffset_inf = (note - index) * noteVal;
    //                 index_inf = index;
    //                 break;
    //             }
    //         }
    //     }
    //     if (index_sup < index_inf) {
    //         vOffset = vOffset_sup;
    //     } else {
    //         vOffset = vOffset_inf;
    //     }
    // }
    // int cvOut = octaveFloat * octVal + vOffset;
}

// int float2int(float f) {
//     int n = 0;
//     if (f >= 0) {
//         int n = (int)(f + 0.5);
//     } else {
//         int n = (int)(f - 0.5);
//     }
//     return n;
// }

void vuMeter(int cv) {
    for (int l = 0; l < 12; l++) {
        if (cv > (ADC_FS/13 * (l + 1))) {
            writeLED(11 - l);
        } else {
            writeLED(-1);
        }
    }
}
