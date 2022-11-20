# Eugenio
Euclidean Sequencer based on Arduino Nano

Features (see `manual/`):
- 4 channels
- external/internal clock options
- external clock divider/multiplier
- trigger and gate output modes, with adjustable gate length for each channel
- reset button and input


## Code

2022-11-19

Changes:
- UI updates
- clock divider/multiplier implemented (to be improved)


### Acknowledgements
- [Tombola](https://modwiggler.com/forum/viewtopic.php?t=45485)'s Euclidean Polyrhythm Generator
- [TimMJN](https://github.com/TimMJN/Arduino-Euclidean-Rhythm-Generator)'s Arduino Euclidean Rhythm Generator


## Hardware

Changes:
- added 10uF decoupling capacitor to LED matrix power input
- Rset value increased to 470 kOhm in order to reduce LED current and induced noise
