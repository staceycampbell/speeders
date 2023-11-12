#include <stdio.h>
#include <ctype.h>
#include <curl/curl.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <assert.h>
#include <libxml/xmlreader.h>
#include "metar.h"

static const char *AviationWeatherFormat = "https://aviationweather.gov/cgi-bin/data/dataserver.php?"
	"requestType=retrieve&"
	"dataSource=metars&"
	"stationString=%s"
	"&startTime=" PRIu64 "&"
	"hoursBeforeNow=0&"
	"format=xml&"
	"mostRecent=true";

#define XML_BUFFER_SIZE 65536
static char XML_buffer[XML_BUFFER_SIZE];
static int XML_buffer_index;

static size_t
ReceiveXMLData(void *buffer, size_t size, size_t nmemb, void *stream)
{
	size *= nmemb;
	assert(XML_buffer_index + size < XML_BUFFER_SIZE);
	strncpy(&XML_buffer[XML_buffer_index], buffer, size);
	XML_buffer_index += size;

	return size;
}

static int32_t
METARFetchNow(const char *station, time_t now, double *temp_c, double *elevation_m)
{
	char url[4096];
	CURL *curlhandle;
	CURLcode curl_status;
	FILE *fp;
	xmlTextReaderPtr reader;
	int reader_status;
	int temp_c_next;
	int elevation_m_next;
	const xmlChar *name, *value;
	static const char *metar_fn = "latestmetar.xml";
	static uint32_t initialized = 0;

	if (! initialized)
	{
		// standard values until METAR data becomes available
		// https://www.grc.nasa.gov/www/k-12/airplane/atmosmet.html
		*temp_c = 15.0;
		*elevation_m = 0.0;
		initialized = 1;
	}

	XML_buffer_index = 0;
	sprintf(url, AviationWeatherFormat, station, (uint64_t)now);

	curlhandle = curl_easy_init();
	curl_easy_setopt(curlhandle, CURLOPT_URL, url);
	curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, ReceiveXMLData);
	memset(XML_buffer, 0, XML_BUFFER_SIZE);
	curl_status = curl_easy_perform(curlhandle);
	curl_easy_cleanup(curlhandle);
	if (curl_status)
	{
		fprintf(stderr, "%s: curl error %d for %s\n", __PRETTY_FUNCTION__, curl_status, url);
		return -1;
	}

	if ((fp = fopen(metar_fn, "w")) == 0)
	{
		fprintf(stderr, "%s: error, cannot write METAR XML %s\n", __PRETTY_FUNCTION__, metar_fn);
		exit(1);
	}
	fputs(XML_buffer, fp);
	fclose(fp);

	reader = xmlReaderForFile(metar_fn, NULL, 0);
	if (reader == 0)
	{
		fprintf(stderr, "%s: unable to read METAR XML file %s\n", __PRETTY_FUNCTION__, metar_fn);
		exit(1);
	}
	temp_c_next = 0;
	elevation_m_next = 0;
	while ((reader_status = xmlTextReaderRead(reader)) == 1)
	{
		name = xmlTextReaderConstName(reader);
		if (!name)
			name = BAD_CAST "--";
		value = xmlTextReaderConstValue(reader);
		if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT)
			if (strcmp((const char *)name, "temp_c") == 0)
				temp_c_next = 1;
			else if (strcmp((const char *)name, "elevation_m") == 0)
				elevation_m_next = 1;
		if (value && xmlTextReaderNodeType(reader) == XML_READER_TYPE_TEXT)
			if (temp_c_next)
			{
				*temp_c = strtod((const char *)value, 0);
				temp_c_next = 0;
			}
			else if (elevation_m_next)
			{
				*elevation_m = strtod((const char *)value, 0);
				elevation_m_next = 0;
			}
	}
	xmlFreeTextReader(reader);
	if (reader_status != 0)
		fprintf(stderr, "Warning: %s parsing problem in METAR XML file %s\n", __PRETTY_FUNCTION__, metar_fn);

	return reader_status;
}

void
METARFetch(const char *station, double *temp_c, double *elevation_m)
{
	time_t now, duration;
	double old_temp, new_temp, new_elevation;
	static double temp_c_cached = 15.0;
	static double elevation_m_cached = 0.0;
	static time_t last_fetch = 0;

	now = time(0);
	duration = now - last_fetch;
	if (duration >= 30 * 60) // don't thrash the server, fetch the temp every 30 minutes
	{
		old_temp = temp_c_cached;
		// Deal with occasional empty or bad xml from data server
		if (METARFetchNow(station, now, &new_temp, &new_elevation) == 0)
		{
			temp_c_cached = new_temp;
			elevation_m_cached = new_elevation;
		}
		last_fetch = now;
		printf("%s (elevation %.1fm) METAR refresh. Old %.1fC, new %.1fC.\n", station, elevation_m_cached, old_temp, temp_c_cached);
	}
	*temp_c = temp_c_cached;
	*elevation_m = elevation_m_cached;
}
