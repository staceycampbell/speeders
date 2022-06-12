# Record aircraft speeding near my Los Angeles neighborhood

In the US the FAA sets what turns out to be a relatively elastic
speed limit of 250 kt for aircraft flying below 10,000 feet MSL.

See [14 CFR § 91.117](https://www.law.cornell.edu/cfr/text/14/91.117).

It's fairly routine to catch aircraft not being compliant with this
rule over the heavily populated San Fernando Valley area of
Los Angeles. Especially over the roof of my house.

This program takes [dump1090-fa](https://github.com/flightaware/dump1090)
Basestation text output from port 30003 and monitors aircraft for
speeders.

Typical usage:

```shell
nc localhost 30003 | speeders
```