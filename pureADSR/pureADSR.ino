/*

GMSN Pure ADSR
Rob Spencer cc-by 4.0

Version 2021-11-10 by Alessio Nuti
- envelope now returns exactly to zero after release
- A, D, R knobs are now linear with time and ranges are adjustable
- logic goes to the correct phase when switching out of trapz mode
- knob order in trapz mode is attack, on, off, release, so that attack and release knobs are the same in all modes

*/

#include "SPI.h"

//Setup pin variables
const byte dacCS = 10;
const byte trigIn = 2;
const byte trigBut = 4;
const byte modeSw1 = 6;
const byte modeSw2 = 7;

const byte knob1 = A3;
const byte knob2 = A2;
const byte knob3 = A1;
const byte knob4 = A0;

//Setup knob ranges (values approximately in seconds@full scale)
const float rangeAttack = 2;
const float rangeDecay = 3;
const float rangeRelease = 3;
const float rangeTrapz = 5;

//Setup envelope variable
float enVal = 0;

//Setup state variables
boolean gate = 0, rising = 0;
byte mode, phase = 0;
unsigned long trapOnCount = 0, trapOffCount = 0;

//Setup switch debounce variables
int buttonState, lastButtonState = HIGH;
unsigned long lastDebounceTime = 0, debounceDelay = 50;

void setup() {
    //Start DAC Comms
    SPI.begin();
    SPI.setBitOrder(MSBFIRST);

    //Configure Pins
    pinMode(dacCS, OUTPUT);   //DAC CS
    pinMode(trigIn, INPUT);   //TRIGGER IN
    pinMode(trigBut, INPUT);  //Button
    pinMode(modeSw1, INPUT);  //MODE SW1
    pinMode(modeSw2, INPUT);  //MODE SW2
    digitalWrite(dacCS, HIGH);

    /*Attach Interrupt for Trigger / Gate in. The button also calls this routine but is polled in the main loop() rather than attached to an interrupt, 
  the hardware design does have it connected to an interrupt pin as well, but during an Interrupt call the millis() function is disabled, which interferes with the debouce routine,
  hence having the button polled*/
    attachInterrupt(digitalPinToInterrupt(trigIn), gateOn, FALLING);
}

void loop() {
    /*The main loop() loops round continuously, everytime it goes round it calculates the current envelope value and writes it to the DAC.
    There's some logic to figure out what Mode it's in and what phase it's in, which tell's it whether it should be calculating an attack value, etc.
    Modes dependant on the Mode Switch. 1 = ADSR, 2 = AR, 3 = Looping Trapezoidal.*/

    //Poll the Mode switch to set the mode
    if (digitalRead(modeSw2) == HIGH) {
        mode = 1;
    } else if (digitalRead(modeSw1) == LOW && digitalRead(modeSw2) == LOW) {
        mode = 2;
    } else {
        mode = 3;
    }

    /* The flow through the phases are dependent on what mode the module is in.
  ADSR:
    Phase 1 = Attack
    Phase 2 = Decay to sustain
    Phase 3 = Release
    
    In this mode, the lack of a Gate In or the Button not being down, moves the modules into the Release Phase.
    So in performance terms, a Gate On moves through Attack, Decay and holds at the Sustain Level.
    When the Gate is removed, say by taking a finger off the key on the keyboard, the envelope moves into Release.
    
    This is the only mode which use the Gate, the other two treat the leading edge of the Gate as a Trigger.
    Any new Gate or Trigger will restart the envelope from zero.
  
  AR:
    Phase 1 = Attack
    Phase 3 = Release
    
    The leading edge of a Gate or Trigger starts the Attack phase, once the envelope reaches the max value, it moves into the Release Phase.
    The Gate or Trigger has no other function, accept to start the envelope running. Any new Gate or Trigger will restart the envelope from zero.
    
  Trap:
    Phase 1 = Attack
    Phase 4 = On
    Phase 3 = Release
    Phase 5 = Off
    
    The phase numbers are intentionally out of sequence. Ideally the Trap phases would be 4, 5, 6 & 7, however the Attack and Release are re-used, so it made sense to do it this way.
    
    The Trap mode loops round, so it can be shaped as some sort of weird LFO, all phases can be taken down to ultashort and snappy or ultralong, over 8 minutes for each phase.
    With all controls set to minimum, the envelope will cycle so quickly it is in the audio range, so crazy waveforms can be created. Set all controls to near maximum and long plus 30 min 
    envelopes can be used for evolving patches. 
   
   Phase Control Logic
     The mechanics of each phase have been kept seperate from the control logic, with each phase having it's own function.
     The control logic is a simple switch() statement that reads which phase the envelope is in, enters that function, does the envelope value calculation and writes the value to the DAC.
     
    */

    //Control logic which calls the functions
    switch (phase) {
        //Phase 0 is idle, but if it's in the Trap mode it kicks it back into the Attack phase.
        case 0:
            if (mode == 3) {
                phase = 1;
            }
            break;

        //Attack Phase with some logic to kick it into the correct next phase depending on the Mode.
        case 1:
            attack();
            if (enVal >= 4095) {
                switch (mode) {
                    case 1:
                        phase = 2;
                        break;

                    case 2:
                        phase = 3;
                        break;

                    case 3:
                        phase = 4;
                        break;
                }
            }

            if (mode == 1 && digitalRead(trigBut) == HIGH && digitalRead(trigIn) == HIGH) {
                phase = 3;
            }
            break;

        //Decay to Sustain Phase.
        case 2:
            decaySustain();
            if (mode == 1 && digitalRead(trigBut) == HIGH && digitalRead(trigIn) == HIGH) {
                phase = 3;
            }
            break;

        //Release Phase.
        case 3:
            releasePhase();
            if (enVal <= 0) {
                enVal = 0;
                if (mode == 3) {
                    phase = 5;
                } else {
                    phase = 0;
                }
            }
            break;

        //Phase 4, Trap On
        case 4:
            trapOn();
            if (mode != 3) {
                phase = 3;
            }
            break;

        //Phase 5, Trap Off
        case 5:
            trapOff();
            if (mode != 3) {
                phase = 0;
            }
            break;
    }

    //Poll Trigger Button, debounce, initialise Up phase
    trigButton();
}

void attack() {
    int aPot = analogRead(knob1);
    float aVal = aPot * 6 * rangeAttack;
    if (aPot < 2) {
        enVal = 4095;
    } else {
        enVal += (6000 - enVal) / aVal;
        if (enVal > 4094) {
            enVal = 4095;
        }
    }
    dacWrite((int)enVal);
}

void decaySustain() {
    int dPot = analogRead(knob2);
    float dVal = dPot * 2.7 * rangeDecay;
    int sPot = analogRead(knob3);
    if (sPot < 2) {
        sPot = 0;
    } else if (sPot > 4093) {
        sPot = 4095;
    }
    float sVal = sPot / 1023.0 * 4095.0;

    if (dPot < 2) {
        enVal = sVal;
    } else {
        enVal += (sVal - enVal) / dVal;
        if (enVal < sVal + 1) {
            enVal = sVal;
        }
    }

    dacWrite((int)enVal);
}

void releasePhase() {
    int rPot = analogRead(knob4);
    float rVal = rPot * 4 * rangeRelease;
    if (rPot < 5) {
        enVal = 0;
    } else {
        enVal -= enVal / rVal;
        if (enVal < 1) {
            enVal = 0;
        }
    }
    dacWrite((int)enVal);
}

void trapOn() {
    dacWrite((int)enVal);
    int onPot = analogRead(knob2);
    float onVal = pow(onPot / 1023.0, 3) * 6500 * rangeTrapz;
    if (trapOnCount >= onVal) {
        trapOnCount = 0;
        phase = 3;
    } else {
        trapOnCount++;
    }
}

void trapOff() {
    dacWrite((int)enVal);
    int offPot = analogRead(knob3);
    float offVal = pow(offPot / 1023.0, 3) * 6500 * rangeTrapz;
    if (trapOffCount >= offVal) {
        trapOffCount = 0;
        phase = 1;
    } else {
        trapOffCount++;
    }
}

/*Debounce routine for the switch. Switches are very noises when they are switching.
When you press a switch, the mechanism can "bounce" on, off, on, off, on, etc, in the space of a few milliseconds.
This debounce function removes the noise.*/
void trigButton() {
    int reading = digitalRead(trigBut);

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == LOW) {
                gateOn();
                lastDebounceTime = millis();
            }
        }
    }
    lastButtonState = reading;
}

//Interrupt routine for rising edge of Gate
void gateOn() {
    if (phase == 1) {
        enVal = 1;
    } else {
        phase = 1;
    }
    trapOffCount = 0;
    trapOnCount = 0;
}

/*Writing to the DAC.
The whole purpose of the module is to output a voltage envelope. That voltage then controls another module, such as a VCA or a filter.
In order to get that voltage out, we need to convert the value of the "envValue" variable into a voltage, which is done by a Digital to Analogue Converter, or DAC for short.
The function below send envValue to the DAC for conversion to the analogue voltage.
0 = Off 4095 = Full on.*/
void dacWrite(int value) {
    digitalWrite(dacCS, LOW);

    //DAC1 write
    //set top 4 bits of value integer to data variable
    byte data = value >> 8;
    data = data & B00001111;
    data = data | B00110000;
    SPI.transfer(data);

    data = value;
    SPI.transfer(data);

    digitalWrite(dacCS, HIGH);
}
