# Record aircraft speeding near my Los Angeles neighborhood

This program takes [dump1090-fa](https://github.com/flightaware/dump1090)
Basestation text output from port 30003 and monitors aircraft for
speeders.

Typical usage:

```shell
nc localhost 30003 | speeders
```

In the US the FAA sets what turns out to be a relatively elastic
speed limit of 250 kt for aircraft flying below 10,000 feet MSL.

See [14 CFR § 91.117](https://www.law.cornell.edu/cfr/text/14/91.117).

Indicated speed is recorded at the aircraft with pitot tubes, so thanks
to "atmospheric conditions" there will be a difference with the GPS
groundspeed sent from the ADS-B system on the aircraft. The program
allows for a 10% faster groundspeed; e.g. the aircraft might have a
groundspeed of 275 kt but an indicated airspeed of 250 kt. We'll cut
them some slack.

It's fairly routine to catch aircraft not being compliant with this
rule over the heavily populated San Fernando Valley area of
Los Angeles. Especially over the roof of my house.

