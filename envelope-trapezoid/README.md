# Envelope/Trapezoid

## GMSN Pure ADSR Code
Rob Spencer cc-by 4.0

Version 2021-11-12 Alessio Nuti
Release Notes:
- envelope now returns exactly to zero after release
- A, D, R knobs are now linear with time and ranges are adjustable
- logic goes to the correct phase when switching out of trapz mode
- knob order in trapz mode is attack, on, off, release, so that attack and release knobs are the same in all modes
- attack and release are linear in trapz mode, exp in ADSR/AR modes
- better control of short trapz on/off times (useful to create shaped audio frequency oscillations)