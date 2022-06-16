#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include "castotas.h"

// Upper left and lower right coordinates of area where speeders
// will be reported
#define NW_LAT   34.23962554621634
#define NW_LON -118.64947656671313
#define SE_LAT   34.13681559402575
#define SE_LON -118.35026457836128

// ...including anywhere within Y miles of these coords
// (currently intersection of Roscoe and Reseda Blvds)
#define ZERO_LAT   34.2207384709914
#define ZERO_LON -118.5360978679256
#define ZERO_WITHIN 6.0 // miles

#define FAA_SPEED_LIMIT_CAS 250 // FAA indicated speed limit in kt
#define FAA_SPEED_ALTITUDE 10000 // ...at or below this MSL altitude in ft

// ADS-B reported speed is groundspeed via GPS unit, https://aerotoolbox.com/airspeed-conversions
#define NAUGHTY_SPEED_CAS (FAA_SPEED_LIMIT_CAS + 10) // give them some slack
#define NAUGHTY_ALTITUDE (FAA_SPEED_ALTITUDE - 750) // give 'em a break over this altitude

#define PLANE_COUNT 1024 // never more than about 70 planes visible from the casa
#define CALLSIGN_LEN 16

static const char BotToken[] = "token.secret";

typedef struct fastest_t {
	uint32_t initialized;
	double naughty;
	int32_t naughty_speed_tas;
	int32_t speed;
	int32_t altitude;
	double distance;
	float latitude;
	float longitude;
	time_t seen;
} fastest_t;

typedef struct plane_t {
	uint32_t valid;
	uint32_t speeder;
	uint32_t icao;
	time_t last_seen;
	time_t last_speed;
	time_t last_location;
	char callsign[CALLSIGN_LEN];
	uint32_t latlong_valid;
	float latitude;
	float longitude;
	int32_t speed;
	int32_t altitude;
	int32_t naughty_speed_tas;
	fastest_t fastest;
} plane_t;

static int PlaneListCount;
static double ZeroLatRadians, ZeroLonRadians;

static char *Quotes[1024];
static int QuoteCount;

static void
QuoteLoad(void)
{
	FILE *fp;
	int len;
	char buffer[2048];
	
	if ((fp = fopen("quotes.txt", "r")) == 0)
	{
		fprintf(stderr, "%s: cannot read quotes.txt\n", __PRETTY_FUNCTION__);
		exit(1);
	}
	QuoteCount = 0;
	while (QuoteCount < sizeof(Quotes) / sizeof(Quotes[0]) && fgets(buffer, sizeof(buffer), fp))
	{
		len = strlen(buffer);
		buffer[len - 1] = '\0';
		assert((Quotes[QuoteCount] = malloc(len + 1)) != 0);
		strcpy(Quotes[QuoteCount], buffer);
		++QuoteCount;
	}
}

static char *
QuotePicker(int32_t speed, int32_t naughty_speed_tas)
{
	int quote_index;
	double quote_index_f;
	char *quote;
	static const int32_t super_fast = 330; // assume nobody is going much faster than this

	// Quotes are ordered by snark. The faster the speed the snarkier the quote.
	quote_index_f = (double)(speed - naughty_speed_tas) / (double)(super_fast - naughty_speed_tas) * (double)QuoteCount;
	quote_index = quote_index_f + 0.5;
	if (quote_index < 0)
		quote_index = 0;
	else
		if (quote_index >= QuoteCount)
			quote_index = QuoteCount - 1;
	quote = Quotes[quote_index];

	return quote;
}

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

	theta = lon1 - lon2;
	dist = sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(theta);
	dist = acos(dist);
	dist = rad2deg(dist);
	dist = dist * 60.0 * 1.1515;

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

	if (plane->speed >= 600)
		return; // bad squitter

	// zone of interest is the rectangle defined by the xx_LAT,xx_LON defines plus within Y miles of ZeroLat,ZeroLon
	lat_radians = DegreesToRadians(plane->latitude);
	lon_radians = DegreesToRadians(plane->longitude);
	dist = CalcDistance(ZeroLatRadians, ZeroLonRadians, lat_radians, lon_radians);
	if ((plane->latitude > NW_LAT || plane->latitude < SE_LAT || plane->longitude > SE_LON || plane->longitude < NW_LON) &&
	    dist > ZERO_WITHIN) // miles
		return; // outside the zone of interest
	
	naughty = ((double)plane->speed - (double)plane->naughty_speed_tas) / (double)plane->naughty_speed_tas;
	naughty *= 100.0;
	if (plane->speeder == 0 || naughty > plane->fastest.naughty)
	{
		plane->speeder = 1;
		plane->fastest.naughty = naughty;
		plane->fastest.speed = plane->speed;
		plane->fastest.altitude = plane->altitude;
		plane->fastest.naughty_speed_tas = plane->naughty_speed_tas;
		plane->fastest.seen = time(0);
		plane->fastest.distance = dist;
		plane->fastest.latitude = plane->latitude;
		plane->fastest.longitude = plane->longitude;
	}

}

static void
DetectBadPlanes(plane_t planes[PLANE_COUNT])
{
	int i;

	for (i = 0; i < PlaneListCount; ++i)
		if (planes[i].valid && planes[i].latlong_valid && planes[i].altitude <= NAUGHTY_ALTITUDE &&
		    planes[i].speed >= planes[i].naughty_speed_tas)
			RecordBadPlane(&planes[i]);
}

static plane_t *
InsertPlane(plane_t planes[PLANE_COUNT], uint32_t icao)
{
	int i;

	i = 0;
	while (i < PLANE_COUNT && planes[i].valid)
		++i;
	assert(i < PLANE_COUNT); // if this pops something incredibly strange is happening
	if (i > PlaneListCount)
		PlaneListCount = i;

	planes[i].valid = 1;
	planes[i].speeder = 0;
	planes[i].icao = icao;
	planes[i].last_seen = 0;
	planes[i].last_speed = 0;
	planes[i].last_location = 0;
	strcpy(planes[i].callsign, "unknown ");
	planes[i].latlong_valid = 0;
	planes[i].speed = -1;
	planes[i].altitude = -100000;

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
	plane->latlong_valid = 1;
	plane->altitude = altitude;
	plane->latitude = lat;
	plane->longitude = lon;
	plane->naughty_speed_tas = CAStoTAS(NAUGHTY_SPEED_CAS, altitude);
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
	strncpy(plane->callsign, ch, sizeof(plane->callsign) - 1);
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
ReportBadPlane(plane_t *plane, int enable_bot)
{
	FILE *fp;
	char *quote;
	int i;
	int system_status;
	char callsign_trimmed[CALLSIGN_LEN];
	char filename[256];
	char command[1024];
	static int fn_inc = 0;
	
	printf("%X %s %d %d %4.1f %8.4f %8.4f (nv %4.1f, tas est %d) %s",
	       plane->icao,
	       plane->callsign,
	       plane->fastest.altitude,
	       plane->fastest.speed,
	       plane->fastest.distance,
	       plane->fastest.latitude,
	       plane->fastest.longitude,
	       plane->fastest.naughty,
	       plane->fastest.naughty_speed_tas,
	       ctime(&plane->fastest.seen));
	if (enable_bot)
	{
		sprintf(filename, "/tmp/bisbadplane%ld-%d.py", (long int)getpid(), fn_inc);
		++fn_inc;
		if ((fp = fopen(filename, "w")) == 0)
		{
			perror(__PRETTY_FUNCTION__);
			exit(1);
		}
		for (i = 0; i < CALLSIGN_LEN; ++i)
			if (plane->callsign[i] != ' ')
				callsign_trimmed[i] = plane->callsign[i];
			else
				callsign_trimmed[i] = '\0';
		
		quote = QuotePicker(plane->fastest.speed, plane->fastest.naughty_speed_tas);
		
		fprintf(fp, "from mastodon import Mastodon\n");
		fprintf(fp, "mastodon = Mastodon(\n    access_token = '%s',\n    api_base_url = 'https://botsin.space/'\n)\n", BotToken);
		fprintf(fp,
			"mastodon.status_post(\"BLEEP BLOOP: I just saw an aircraft with callsign %s (ICAO code %X) flying at %d kt "
			"at altitude %d feet MSL at coordinates %8.4f,%8.4f.\\n\\n%s\\n\\n"
			"https://globe.adsbexchange.com/?icao=%x\")\n",
			callsign_trimmed,
			plane->icao,
			plane->fastest.speed,
			plane->fastest.altitude,
			plane->fastest.latitude,
			plane->fastest.longitude,
			quote,
			plane->icao);
		fclose(fp);
		sprintf(command, "/usr/bin/python3 %s", filename);
		system_status = system(command);
		if (system_status)
			fprintf(stderr, "%s: system(%s) returned status %d\n", __PRETTY_FUNCTION__, command, system_status);
		else
			unlink(filename);
	}
}


static void
CleanPlanes(plane_t planes[PLANE_COUNT], int enable_bot)
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
					ReportBadPlane(&planes[i], enable_bot);
				planes[i].valid = 0;
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	int i, opt, enable_bot, usage;
	uint32_t message_id, icao;
	char buffer[1024];
	char *p;
	char *ch;
	plane_t planes[PLANE_COUNT];

	enable_bot = 0;
	usage = 0;
	while ((opt = getopt(argc, argv, "b")) != EOF)
		switch (opt)
		{
		case 'b' :
			enable_bot = 1;
			break;
		default :
			usage = 1;
			break;
		}
	if (usage)
	{
		fprintf(stderr, "usage: %s [-b]\n", argv[0]);
		fprintf(stderr, "\t-b = enable bot reporting\n\n");
		fprintf(stderr, "\texample usage: nc localhost 30003 | %s\n", argv[0]);
		
		return 1;
	}

	if (enable_bot)
	{
		struct stat statbuf;
		
		if (stat(BotToken, &statbuf))
		{
			fprintf(stderr, "%s: cannot stat bot token file %s\n", argv[0], BotToken);
			exit(1);
		}
		QuoteLoad();
	}

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
		CleanPlanes(planes, enable_bot);
		DetectBadPlanes(planes);
	}

	return 0;
}
