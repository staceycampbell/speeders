#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define PLANE_COUNT 200

typedef struct plane_t {
	uint32_t valid;
	uint32_t icao;
	time_t last_seen;
	time_t last_speed;
	time_t last_location;
	time_t last_altitude;
	char callsign[64];
	uint32_t latlong_valid;
	double latitude;
	double longitude;
	int32_t speed;
	int32_t altitude;
} plane_t;

static int PlaneListCount;

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
	planes[i].last_altitude = 0;
	memset(planes[i].callsign, 0, sizeof(planes[i].callsign));
	planes[i].latlong_valid = 0;
	planes[i].speed = -1;
	planes[i].altitude = -1;
	printf("%X added\n", icao);

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
ProcessPlane(plane_t planes[PLANE_COUNT], uint32_t message_id, uint32_t icao)
{
	plane_t *plane;

	plane = FindPlane(planes, icao);
}

static void
CleanPlanes(plane_t planes[PLANE_COUNT])
{
	int i, plane_count;
	time_t now, duration;
	static int old_plane_count = -1;

	plane_count = 0;
	now = time(0);
	for (i = 0; i < PlaneListCount; ++i)
	{
		if (planes[i].valid)
		{
			++plane_count;
			duration = now - planes[i].last_seen;
			if (duration > 10)
			{
				planes[i].valid = 0;
				printf("%X removed\n", planes[i].icao);
			}
		}
	}
	if (plane_count != old_plane_count)
	{
		printf("%d planes on list, list length %d\n", plane_count, PlaneListCount);
		old_plane_count = plane_count;
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
	}

	return 0;
}
