#include <stdio.h>
#include <stdint.h>
#include "castotas.h"
#include "metar.h"

int
main(void)
{
	int32_t tas, cas, altitude;
	double temp_c = 15.0, elevation_m = 0.0;
#if 0
	static char *station = "KVNY";

	METARFetch(station, &temp_c, &elevation_m);
	printf("Temp at %s is %.1f, elevation is %.1f\n", station, temp_c, elevation_m);
#endif
	
	cas = 250; // aircraft calibrated air speed
	elevation_m = 241.0; // METAR station elevation
	altitude = 5000; // aircraft altitude
	for (temp_c = 10.0; temp_c <= 50.0; temp_c += 1)
	{
		tas = CAStoTAS(temp_c, elevation_m, cas, altitude);
		printf("temp=%.1f cas=%d altitude=%d : tas=%d\n", temp_c, cas, altitude, tas);
	}

	return 0;
}
