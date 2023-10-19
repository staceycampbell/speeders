#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "castotas.h"
#include "metar.h"
#include "datetoepoch.h"

int
main(void)
{
	double temp_c = 15.0, elevation_m = 0.0;
	static char *station = "KVNY";

	METARFetch(station, &temp_c, &elevation_m);
	printf("Temp at %s is %.1f, elevation is %.1f\n", station, temp_c, elevation_m);

#if 0
	int32_t tas, cas, altitude;

	cas = 250; // aircraft calibrated air speed
	elevation_m = 241.0; // METAR station elevation
	altitude = 5000; // aircraft altitude
	for (temp_c = 10.0; temp_c <= 50.0; temp_c += 1)
	{
		tas = CAStoTAS(temp_c, elevation_m, cas, altitude);
		printf("temp=%.1f cas=%d altitude=%d : tas=%d\n", temp_c, cas, altitude, tas);
	}
#endif

#if 0
	time_t t;
	char s0[] = "2022/06/19";
	char s1[] = "07:46:03.649";

	t = Date2Epoch(s0, s1);
	printf("%s,%s %ld %s", s0, s1, t, ctime(&t));
#endif

	return 0;
}
