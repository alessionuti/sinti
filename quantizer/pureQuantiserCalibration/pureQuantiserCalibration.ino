/*
  GMSN Pure Quantiser Calibration Utility
  2021-11-15
  Alessio Nuti

  Features:
  1) bypass mode:
    - output (DAC counts) = input (ADC counts)

  2) DAC calibration mode:
    - press button C, F or B to enter DAC calibration mode
    - C active -> DAC output 4095
    - F active -> DAC output 2048
    - B active -> DAC output 0
  
*/

#include <EEPROM.h>

#include "SPI.h"

//Setup LED Pin Varibles
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
int mode = 0;  // 0 input  // 1 output
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
}

void loop() {
    //The main loop() is made of two part.
    // 1) Button Management. Each loop through reads and lights one of the 12 buttons.
    // 2) CV Management. Each loop reads the CV In, then depending on the mode, will either Quantiser the CV and look after the Trigger, or will pass the CV straight through with no trigger.

    //1) Button Management

    //Read Buttons to see if they are being pressed in this cycle. As the program loops round it will check if a button us currently being pressed and turn on the relevant flag within the notes array.
    readNoteButton(i);

    //Increment the counter so on the next loop we'll read and light the next LED.
    if (i > 11) {
        i = 0;
    } else {
        i++;
    }

    //2) CV Management

    //Read CV In
    cvIn = adcRead(0);

    //Check all the note flags and if none are set, we're in output mode, mode=0
  
    if (notes[0] || notes[5] || notes[11]) {
        mode = 1;
    }
    else{
      mode = 0;
    }

    //If mode=0, just output the input and flash the leds like a VU Meter.
    if (mode == 0) {
        writeLED(-1);
        cvOut = cvIn;
    }

    else {
        //If not in mode=0, Output fixed CVs
        if (notes[0]) {
            cvOut = 4095;
            writeLED(0);
        } else if (notes[5]) {
            cvOut = 2048;
            writeLED(5);
        } else if (notes[11]) {
            cvOut = 0;
            writeLED(11);
        } else {
            writeLED(-1);
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

