#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>

#define NW_LAT 34.20336236633384
#define NW_LON -118.65003500040142
#define SE_LAT 34.13874066806624
#define SE_LON -118.55910304337476

#define ZERO_LAT 34.17074351801564
#define ZERO_LON -118.60582458967741

#define FAA_SPEED_LIMIT 250 // FAA speed limit in kt
#define FAA_SPEED_ALTITUDE 10000 // ...at or below this altitude

#define NAUGHTY_SPEED (FAA_SPEED_LIMIT + 10) // fast!
#define NAUGHTY_ALTITUDE (FAA_SPEED_ALTITUDE - 1000) // low!

#define PLANE_COUNT 200

typedef struct fastest_t {
	double naughty;
	int32_t speed;
	int32_t altitude;
	time_t seen;
	double distance;
	float latitude;
	float longitude;
} fastest_t;

typedef struct plane_t {
	uint32_t valid;
	uint32_t speeder;
	uint32_t icao;
	time_t last_seen;
	time_t last_speed;
	time_t last_location;
	char callsign[64];
	uint32_t latlong_valid;
	float latitude;
	float longitude;
	int32_t speed;
	int32_t altitude;
	fastest_t fastest;
} plane_t;

static int PlaneListCount;
static double ZeroLatRadians, ZeroLonRadians;

static double
DegreesToRadians(double d)
{
	double r;

	r = (d * M_PI) / 180.0;

	return r;
}

static double
rad2deg(double rad)
{
	return rad * 180.0 / M_PI;
}

static double
CalcDistance(double lat1, double lon1, double lat2, double lon2)
{
	double theta, dist;
	static const char unit = 'M';

	theta = lon1 - lon2;
	dist = sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(theta);
	dist = acos(dist);
	dist = rad2deg(dist);
	dist = dist * 60.0 * 1.1515;
	switch (unit)
	{
	default:
	case 'M':
		break;
	case 'K':
		dist = dist * 1.609344;
		break;
	case 'N':
		dist = dist * 0.8684;
		break;
	}

	return dist;
}

static void
RecordBadPlane(plane_t *plane)
{
	time_t speed_alt_time_gap;
	double dist;
	double lat_radians, lon_radians;
	double naughty;

	speed_alt_time_gap = plane->last_speed - plane->last_location;
	if (speed_alt_time_gap < 0)
		speed_alt_time_gap = -speed_alt_time_gap;
	if (speed_alt_time_gap >= 3)
		return; // long gap between altitude and speed recording times, might not have been speeding

	if (plane->altitude <= 0)
		return; // yikes

	if (plane->latitude > NW_LAT || plane->latitude < SE_LAT ||
	    plane->longitude > SE_LON || plane->longitude < NW_LON)
		return; // outside the zone of interest
	
	lat_radians = DegreesToRadians(plane->latitude);
	lon_radians = DegreesToRadians(plane->longitude);
	dist = CalcDistance(ZeroLatRadians, ZeroLonRadians, lat_radians, lon_radians);

	naughty = (((double)FAA_SPEED_ALTITUDE / (double)plane->altitude) + (plane->speed / (double)FAA_SPEED_LIMIT)) / 2.0;
	
	if (naughty > plane->fastest.naughty)
	{
		plane->fastest.naughty = naughty;
		plane->fastest.speed = plane->speed;
		plane->fastest.altitude = plane->altitude;
		plane->fastest.seen = time(0);
		plane->fastest.distance = dist;
		plane->fastest.latitude = plane->latitude;
		plane->fastest.longitude = plane->longitude;
	}

	plane->speeder = 1;
}

static void
DetectBadPlanes(plane_t planes[PLANE_COUNT])
{
	int i;

	for (i = 0; i < PlaneListCount; ++i)
		if (planes[i].valid && planes[i].latlong_valid && planes[i].altitude < NAUGHTY_ALTITUDE && planes[i].speed >= NAUGHTY_SPEED)
			RecordBadPlane(&planes[i]);
}

static plane_t *
InsertPlane(plane_t planes[PLANE_COUNT], uint32_t icao)
{
	int i;

	i = 0;
	while (i < PLANE_COUNT && planes[i].valid)
		++i;
	assert(i < PLANE_COUNT);
	if (i > PlaneListCount)
		PlaneListCount = i;

	planes[i].valid = 1;
	planes[i].speeder = 0;
	planes[i].icao = icao;
	planes[i].last_seen = 0;
	planes[i].last_speed = 0;
	planes[i].last_location = 0;
	memset(planes[i].callsign, 0, sizeof(planes[i].callsign));
	planes[i].latlong_valid = 0;
	planes[i].speed = -1;
	planes[i].altitude = -100000;

	planes[i].fastest.naughty = 0;
	planes[i].fastest.speed = 0;
	planes[i].fastest.altitude = 0;
	planes[i].fastest.seen = 0;

	return &planes[i];
}

static plane_t *
FindPlane(plane_t planes[PLANE_COUNT], uint32_t icao)
{
	int i;
	plane_t *plane;

	i = 0;
	while (i < PlaneListCount && ! (planes[i].icao == icao && planes[i].valid))
		++i;
	if (i == PlaneListCount)
		plane = InsertPlane(planes, icao);
	else
		plane = &planes[i];

	plane->last_seen = time(0);

	return plane;
}

static void
ProcessMSG3(char **pp, plane_t *plane)
{
	char *ch;
	int field;
	int32_t altitude;
	float lat, lon;

	field = 0;
	while ((ch = strsep(pp, ",")) && field < 6)
		++field;
	if (ch == 0)
		return;
	altitude = strtol(ch, 0, 10);
	if (altitude < -500 || altitude > 100000)
		return;

	field = 0;
	while ((ch = strsep(pp, ",")) && field < 2)
		++field;
	if (ch == 0)
		return;

	sscanf(ch, "%f", &lat);
	ch = strsep(pp, ",");
	if (ch == 0)
		return;
	sscanf(ch, "%f", &lon);
	
	plane->last_location = time(0);
	plane->altitude = altitude;
	plane->latlong_valid = 1;
	plane->latitude = lat;
	plane->longitude = lon;
}

static void
ProcessMSG4(char **pp, plane_t *plane)
{
	char *ch;
	int field;
	int32_t speed;

	field = 0;
	while ((ch = strsep(pp, ",")) && field < 7)
		++field;
	if (ch == 0)
		return;
	
	speed = strtol(ch, 0, 10);
	if (speed <= 0 || speed > 3000)
		return;

	plane->last_speed = time(0);
	plane->speed = speed;
}

static void
ProcessMSG1(char **pp, plane_t *plane)
{
	char *ch;
	int field;

	field = 0;
	while ((ch = strsep(pp, ",")) && field < 5)
		++field;
	if (ch == 0 || *ch == '\0')
		return;
	strncpy(plane->callsign, ch, sizeof(plane->callsign));
}

static void
ProcessPlane(char **pp, plane_t planes[PLANE_COUNT], uint32_t message_id, uint32_t icao)
{
	plane_t *plane;

	plane = FindPlane(planes, icao);
	switch (message_id)
	{
	case 1 :
		ProcessMSG1(pp, plane);
		break;
	case 3 :
		ProcessMSG3(pp, plane);
		break;
	case 4 :
		ProcessMSG4(pp, plane);
		break;
	}
}

static void
ReportBadPlane(plane_t *plane)
{
	printf("%X %s %d %d %f %f %f (naughty %f)\n", plane->icao, plane->callsign,
	       plane->fastest.altitude,
	       plane->fastest.speed,
	       plane->fastest.distance,
	       plane->fastest.latitude,
	       plane->fastest.longitude,
	       plane->fastest.naughty);
}


static void
CleanPlanes(plane_t planes[PLANE_COUNT])
{
	int i;
	time_t now, duration;

	now = time(0);
	for (i = 0; i < PlaneListCount; ++i)
	{
		if (planes[i].valid)
		{
			duration = now - planes[i].last_seen;
			if (duration > 10)
			{
				if (planes[i].speeder)
					ReportBadPlane(&planes[i]);
				planes[i].valid = 0;
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	int i;
	uint32_t message_id, icao;
	char buffer[1024];
	char *p;
	char *ch;
	plane_t planes[PLANE_COUNT];

	for (i = 0; i < PLANE_COUNT; ++i)
		planes[i].valid = 0;
	PlaneListCount = 0;
	ZeroLatRadians = DegreesToRadians(ZERO_LAT);
	ZeroLonRadians = DegreesToRadians(ZERO_LON);

	while (fgets(buffer, sizeof(buffer), stdin))
	{
		p = buffer;
		ch = strsep(&p, ",");
		if (ch && strncmp(ch, "MSG", 3) == 0)
		{
			ch = strsep(&p, ",");
			if (ch)
			{
				message_id = strtoul(ch, 0, 0);
				ch = strsep(&p, ",");
				if (ch)
				{
					ch = strsep(&p, ",");
					if (ch)
					{
						ch = strsep(&p, ",");
						icao = strtoul(ch, 0, 16);
						ProcessPlane(&p, planes, message_id, icao);
					}
				}
			}
		}
		CleanPlanes(planes);
		DetectBadPlanes(planes);
	}

	return 0;
}
