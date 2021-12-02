# Quantizer

## GMSN Pure Quantiser Code
Rob Spencer cc-by 4.0

Version 2021-11-19 Alessio Nuti

Features:
- new quantization algorithm: quantize to the nearest active note
- support for ADC calibration

## Hardware
### Recommended hardware mods (input range 0V -> 10V):
  - R19, R20, R21 = 220R
  - R3 = 4K7
  - R4 = 100R
  - R12 = 100K
  - R10 = 47K

### Optional mod (input range -5V -> +5V):
  - connect +5V to the first opamp input through a series of two 47K resistors and a 10K trimmer (TR2); 
  this adds +5V to the input (acceptable cv range is now -5V -> +5V)
  - add a switch if you want to keep the two input range options together 
  - note: the output CV is always 0V -> 10V; when using the -5V -> +5V option notes are transposed by 5 octaves


##  Calibration
  - Upload the "GMSN Pure Quantiser Calibration Utility"
  - Output 4095 DAC
  - Calibrate output to full scale (10V) using TR1
  - Switch to bypass mode, output (DAC counts) = input (ADC counts)
  - Input a known voltage V_in and measure output V_out
  - Calculate ADC_FS = 4095 * V_out/V_in and put the value into code

### GMSN Pure Quantiser Calibration Utility
Features:
1) bypass mode:
  - output (DAC counts) = input (ADC counts)

2) DAC calibration mode:
  - press button C, F or B to enter DAC calibration mode (press again to return to bypass mode)
  - C active -> DAC output 4095
  - F active -> DAC output 2048
  - B active -> DAC output 0
