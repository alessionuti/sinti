# Eugenio Euclidean Rhythm Generator

## Code

2022-02-03 Alessio Nuti

Release Notes:
- new code based on both [TimMJN](https://github.com/TimMJN/Arduino-Euclidean-Rhythm-Generator) and [Tombola](https://modwiggler.com/forum/viewtopic.php?t=45485) implementations
- 4 channels
- all encoders working (1 = length, 2 = hits, 3 = offset)
- all switches working (1 = switch channel, 2 = enter clock divider controlled by potentiometer, 3 = sleep display)
- clock managed by interrupts
- euclidean algorithm rewritten
- UI with scrolling display (1 line per channel), and a common area for displaying active changes

### Acknowledgements
- [Tombola](https://modwiggler.com/forum/viewtopic.php?t=45485)'s Euclidean Polyrhythm Generator
- [TimMJN](https://github.com/TimMJN/Arduino-Euclidean-Rhythm-Generator)'s Arduino Euclidean Rhythm Generator


## Hardware

Notes:
- added 10uF decoupling capacitor to LED matrix power input
- Rset value increased to 470 kOhm in order to reduce LED current and induced noise
