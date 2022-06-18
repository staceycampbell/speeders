#include <stdio.h>
#include <ctype.h>
#include <curl/curl.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <libxml/xmlreader.h>
#include "metar.h"

static const char *AviationWeatherFormat = "https://www.aviationweather.gov/adds/dataserver_current/httpparam?"
	"dataSource=metars&requestType=retrieve&format=xml&hoursBeforeNow=10&" "mostRecentForEachStation=constraint&stationString=";

#define XML_BUFFER_SIZE 65536
static char XML_buffer[XML_BUFFER_SIZE];

static int
ReceiveXMLData(void *buffer, size_t size, size_t nmemb, void *stream)
{
	size *= nmemb;
	size = (size <= XML_BUFFER_SIZE) ? size : XML_BUFFER_SIZE;
	strncpy(XML_buffer, buffer, size);
	return size;
}

static void
METARFetchNow(const char *station, double *temp_c, double *elevation_m)
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

	*temp_c = 15.0;
	*elevation_m = 0.0;

	strcpy(url, AviationWeatherFormat);
	strcat(url, station);

	curlhandle = curl_easy_init();
	curl_easy_setopt(curlhandle, CURLOPT_URL, url);
	curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, ReceiveXMLData);
	memset(XML_buffer, 0, XML_BUFFER_SIZE);
	curl_status = curl_easy_perform(curlhandle);
	curl_easy_cleanup(curlhandle);
	if (curl_status)
	{
		fprintf(stderr, "%s: curl error %d for %s\n", __PRETTY_FUNCTION__, curl_status, url);
		return;
	}

	if ((fp = fopen(metar_fn, "w")) == 0)
	{
		fprintf(stderr, "%s: error, cannot write METAR XML\n", __PRETTY_FUNCTION__);
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
	{
		fprintf(stderr, "%s: parsing problem in METAR XML file %s\n", __PRETTY_FUNCTION__, metar_fn);
		exit(1);
	}
}

void
METARFetch(const char *station, double *temp_c, double *elevation_m)
{
	time_t now, duration;
	static double temp_c_cached = 15.0;
	static double elevation_m_cached = 0.0;
	static time_t last_fetch = 0;

	now = time(0);
	duration = now - last_fetch;
	if (duration >= 20 * 60) // don't thrash the server, fetch the temp every 20 minutes
	{
		printf("%s METAR temperature cache stale, refreshing it now. Old %.1fC, new ", station, temp_c_cached);
		METARFetchNow(station, &temp_c_cached, &elevation_m_cached);
		last_fetch = now;
		printf("%.1fC (elevation %.1fm).\n", temp_c_cached, elevation_m_cached);
	}
	*temp_c = temp_c_cached;
	*elevation_m = elevation_m_cached;
}
