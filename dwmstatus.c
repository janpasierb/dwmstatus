/*
 * Copy me if you can.
 * by 20h
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

char *tzlondon = "Europe/London";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)

		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char*
strip(char *name)
{
	size_t size = strlen(name) - 1;
	char *ret = malloc(size);
	return strncpy(ret, name, size);
}

char*
gettemperature(char *base, char *sensorNameFile, char *sensorTempFile)
{
	char *sensorName, *sensorTemp;

	sensorName = readfile(base, sensorNameFile);
	sensorTemp = readfile(base, sensorTempFile);

	if(sensorName == NULL || sensorTemp == NULL)
		return smprintf("");
	return smprintf("%s: %02.0f°C", strip(sensorName), atof(sensorTemp) / 1000);
}

int
main(void)
{
	char *status;
	char *tmbln;
	char *tpac, *tc1, *tc2, *tc3, *tc4;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) {
		tpac = gettemperature("/sys/devices/platform/coretemp.0/hwmon/hwmon0", "temp1_label", "temp1_input");
		tc1 = gettemperature("/sys/devices/platform/coretemp.0/hwmon/hwmon0", "temp2_label", "temp2_input");
		tc2 = gettemperature("/sys/devices/platform/coretemp.0/hwmon/hwmon0", "temp3_label", "temp3_input");
		tc3 = gettemperature("/sys/devices/platform/coretemp.0/hwmon/hwmon0", "temp4_label", "temp4_input");
		tc4 = gettemperature("/sys/devices/platform/coretemp.0/hwmon/hwmon0", "temp5_label", "temp5_input");

		tmbln = mktimes("%W %a %d %b %H:%M:%S %Z %Y", tzlondon);

		status = smprintf("%s|%s|%s|%s|%s   |   Time: %s", tpac, tc1, tc2, tc3, tc4, tmbln);
		setstatus(status);

		free(tpac);
		free(tc1);
		free(tc2);
		free(tc3);
		free(tc4);
		free(tmbln);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

