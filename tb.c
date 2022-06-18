#include <stdio.h>
#include "metar.h"

int
main(void)
{
	double temp_c, elevation_m;
	static char *station = "KVNY";

	MetarFetch(station, &temp_c, &elevation_m);
	printf("Temp at %s is %.1f, elevation is %.1f\n", station, temp_c, elevation_m);

	return 0;
}
