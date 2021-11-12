# Quantizer

## GMSN Pure Quantiser Code
Rob Spencer cc-by 4.0

Version 2021-11-12 Alessio Nuti
Release Notes:

Recommended hardware mods:
  R19, R20, R21 = 220R
  R3 = 4K7
  R4 = 100R
  R12 = 47K + trimmer 5K (series)

## GMSN Pure Quantiser Calibration Utility
2021-11-12
Alessio Nuti

Features:
1) ADC calibration mode:
- ADC read 4095 -> LED C
- ADC read 4094 -> LED C#
- ADC read 4093 -> LED D
- ADC read 2049 -> LED E
- ADC read 2048 -> LED F
- ADC read 2047 -> LED F#
- ADC read 2046 -> LED G
- ADC read 2 -> LED A
- ADC read 1 -> LED A#
- ADC read 0 -> LED B

2) DAC calibration mode:
- press button C, F or B to enter DAC calibration mode
- C active -> DAC output 4095
- F active -> DAC output 2048
- B active -> DAC output 0




