# lighthouse1
Simple Vive Base Station 1.0 tracking

## Introduction

The [vive-diy-position-sensor project](https://github.com/ashtuchkin/vive-diy-position-sensor) outlines a simple circuit utilising a [BPV22NF](https://uk.rs-online.com/web/p/photodiodes/1652447/) infrared photodiode and [TLV2462](https://uk.rs-online.com/web/p/op-amps/3568228/) op-amp that is sufficiently infrared sensitive to detect the infrared sync pulses and sweeps of an original Vive Base Station (version 1.0).

This project is aimed at creating an Arduino compatible code to utilise the vive-diy-position-sensor to locate in 3D space.

## Software overview

The code written in this repository is largely based on the following references:
* https://github.com/ashtuchkin/vive-diy-position-sensor
* https://github.com/nairol/LighthouseRedox/blob/master/docs/Light%20Emissions.md
* https://github.com/nairol/LighthouseRedox/blob/master/docs/Base%20Station.md
These describe the infrared signal timings of the base station sync and sweep, and how these encode information about which light house is broadcasting and which axis the following pulse is for.

In general, the code is structured to be split into two halves. The first half of the code is an interrupt listening for activity on the sensor, triggering on rising and falling. At each fall, the duration and time of the pulse is stored in an array of the last several pulses. The second half of the code operates on the main loop and classifies each pulse. If the pulse is between 59 and 139 microseconds in duration, it is one of eight possible base station sync pulses, which indicates what base station emits the next pulse and what axis it's on. If the pulse is less than 20 microseconds in duration, it is a sweep pulse and its timing between 1222 and 6777 microseconds after the rising limb of the base station A sync pulse indicates the angle (scaled -60 to 60 degrees).

### Current status

Currently the code only works for a single base station and prints out the x and y angle of the sensor relative to the base station. It would probably not be happy with two base stations running. More points and more math is needed in order to get 3D spatial tracking actually working.

## Notes on the circuit

The version of the circuit we have made uses the TLV2462CP instead of the TLV2462IP originally suggested, the only difference is commercial vs industrial operating temperature ratings. It also only uses a single photodiode, this has proven to be sufficiently sensitive for initial testing. We are currently using a Teensy 3.5, but this code should work with any sufficiently fast microprocessor using the Arduino IDE. To achieve 3k ohm resistance we used a 1.2k Ω resistor and a 1.8k Ω resistor in series. We are also operating it at 3.3V logic levels rather than 5V and haven't had any problems - this suggests it would be suitable for a RP2040 or Atmega32u4-based board.
