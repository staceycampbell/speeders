#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>

#define ZERO_LAT 34.17074351801564
#define ZERO_LON -118.60582458967741

#define PLANE_COUNT 200

typedef struct plane_t {
	uint32_t valid;
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
	time_t last_reported;
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
ReportBadPlane(plane_t *plane)
{
	time_t now;
	time_t speed_alt_time_gap;
	double dist;
	double lat_radians, lon_radians;

	if (plane->callsign[0] == '\0')
		return; // speeders will eventually reveal their callsign

	speed_alt_time_gap = plane->last_speed - plane->last_location;
	if (speed_alt_time_gap < 0)
		speed_alt_time_gap = -speed_alt_time_gap;
	if (speed_alt_time_gap >= 3)
		return; // gap between altitude and speed recording times, might not have been speeding
	
	now = time(0);
	if (now - plane->last_reported < 600)
		return; // don't spam the chat

	lat_radians = DegreesToRadians(plane->latitude);
	lon_radians = DegreesToRadians(plane->longitude);
	dist = CalcDistance(ZeroLatRadians, ZeroLonRadians, lat_radians, lon_radians);
	
	plane->last_reported = now;
	printf("%X %s %d %d %f %f %f\n", plane->icao, plane->callsign, plane->altitude, plane->speed, dist, plane->latitude, plane->longitude);
}

static void
DetectBadPlanes(plane_t planes[PLANE_COUNT])
{
	int i;

	for (i = 0; i < PlaneListCount; ++i)
		if (planes[i].valid && planes[i].latlong_valid && planes[i].altitude < 10000 && planes[i].speed > 250)
			ReportBadPlane(&planes[i]);
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
	planes[i].icao = icao;
	planes[i].last_seen = 0;
	planes[i].last_speed = 0;
	planes[i].last_location = 0;
	memset(planes[i].callsign, 0, sizeof(planes[i].callsign));
	planes[i].latlong_valid = 0;
	planes[i].speed = -1;
	planes[i].altitude = -100000;
	planes[i].last_reported = 0;

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
ProcessMSG3(plane_t *plane)
{
	char *ch;
	int field;
	int32_t altitude;
	float lat, lon;

	field = 0;
	while ((ch = strtok(0, ",")) && field < 5)
		++field;
	if (ch == 0)
		return;

	altitude = strtol(ch, 0, 10);
	if (altitude < -500 || altitude > 100000)
		return;
	ch = strtok(0, ",");
	if (ch == 0)
		return;
	sscanf(ch, "%f", &lat);
	ch = strtok(0, ",");
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
ProcessMSG4(plane_t *plane)
{
	char *ch;
	int field;
	int32_t speed;

	field = 0;
	while ((ch = strtok(0, ",")) && field < 5)
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
ProcessMSG1(plane_t *plane)
{
	char *ch;
	int field;

	field = 0;
	while ((ch = strtok(0, ",")) && field < 5)
		++field;
	if (ch == 0 || *ch == '\0')
		return;
	strncpy(plane->callsign, ch, sizeof(plane->callsign));
}

static void
ProcessPlane(plane_t planes[PLANE_COUNT], uint32_t message_id, uint32_t icao)
{
	plane_t *plane;

	plane = FindPlane(planes, icao);
	switch (message_id)
	{
	case 1 :
		ProcessMSG1(plane);
		break;
	case 3 :
		ProcessMSG3(plane);
		break;
	case 4 :
		ProcessMSG4(plane);
		break;
	}
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
				planes[i].valid = 0;
		}
	}
}

int
main(int argc, char *argv[])
{
	int i;
	uint32_t message_id, icao;
	char buffer[1024];
	char *ch;
	plane_t planes[PLANE_COUNT];

	for (i = 0; i < PLANE_COUNT; ++i)
		planes[i].valid = 0;
	PlaneListCount = 0;
	ZeroLatRadians = DegreesToRadians(ZERO_LAT);
	ZeroLonRadians = DegreesToRadians(ZERO_LON);

	while (fgets(buffer, sizeof(buffer), stdin))
	{
		ch = strtok(buffer, ",");
		if (ch && strncmp(ch, "MSG", 3) == 0)
		{
			ch = strtok(0, ",");
			if (ch)
			{
				message_id = strtoul(ch, 0, 0);
				ch = strtok(0, ",");
				if (ch)
				{
					ch = strtok(0, ",");
					if (ch)
					{
						ch = strtok(0, ",");
						icao = strtoul(ch, 0, 16);
						ProcessPlane(planes, message_id, icao);
					}
				}
			}
		}
		CleanPlanes(planes);
		DetectBadPlanes(planes);
	}

	return 0;
}
