# Record aircraft speeding near my Los Angeles neighborhood

This program takes [dump1090-fa](https://github.com/flightaware/dump1090)
BaseStation text output from port 30003 and monitors aircraft for
speeders.

In the US the FAA sets an indicated speed limit of 250 kt for aircraft
flying below 10,000 feet MSL.

See [14 CFR § 91.117](https://www.law.cornell.edu/cfr/text/14/91.117).

It's not unusual to catch aircraft likely not being compliant with this
rule over the heavily populated San Fernando Valley area of Los Angeles.
Especially over the roof of my house.

## Prerequisites

```shell
sudo apt install libcurl4-openssl-dev libxml2-dev python3
```

## Typical usage:

```shell
nc localhost 30003 | speeders
```

## Implementation

Indicated speed is recorded at the aircraft with pitot tubes. Due to
numerous factors (physics, eh?), indicated airspeed will be less than
the airspeed measured by the GPS in the ADS-B system on the aircraft.

This program receives the GPS speed and altitude of the aircraft via
[dump1090-fa](https://github.com/flightaware/dump1090), then
[estimates the indicated airspeed](https://aviation.stackexchange.com/a/64251)
based on the aircraft's altitude. The program checks any time the aircraft's
estimated indicated speed exceeds the FAA indicated speed limit for the
aircraft's altitude. The most egregious departure from the limit is stored
and reported once the aircraft flies out of range.