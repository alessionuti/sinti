# Eugenio Euclidean Rhythm Generator

## Code

2022-03-09 Alessio Nuti

Features:
- 4 channels
- 3 encoders (1 = length, 2 = hits, 3 = offset)
- 3 encoder switches (1 = external/internal clock, 2 = channel selection (hold for display sleep mode), 3 = modify pulse length)
- UI with scrolling display (1 line per channel), and a common area for displaying active changes
- multifunction potentiometer for adjusting output pulse length for every channel and tempo in internal clock mode 

### Acknowledgements
- [Tombola](https://modwiggler.com/forum/viewtopic.php?t=45485)'s Euclidean Polyrhythm Generator
- [TimMJN](https://github.com/TimMJN/Arduino-Euclidean-Rhythm-Generator)'s Arduino Euclidean Rhythm Generator


## Hardware

Notes:
- added 10uF decoupling capacitor to LED matrix power input
- Rset value increased to 470 kOhm in order to reduce LED current and induced noise (the MAX7221 would be better)
