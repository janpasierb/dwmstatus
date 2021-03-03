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
#include <dirent.h>
#include <stdbool.h>

#include <X11/Xlib.h>

#define	LEN	10

static char *hwmon="/home/janek/hwinfo/hwmon";
static char *basedir = "/dev";
static char *filefmtincl = "sd";
static char filefmtexcl[LEN] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'} ;
static char *alab = "Added: ";
static char *rlab = "Removed: ";
static char *sep = ", ";
static char *noblkchg = "-";

char *tzlondon = "Europe/London";

static Display *dpy;

typedef enum {
	ADD, 
	REMOVE,
	NOTHING
} blkstatt;

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

char*
blkprintf(int dvcsize, char **dvcs, blkstatt stat)
{
	char *blkstatus;
	int statsize = 0;

	if(!dvcsize || dvcs == NULL || stat == NOTHING)
	{
		blkstatus = malloc(2);
		if(!blkstatus)
			return NULL;

		strcpy(blkstatus, noblkchg);
		return blkstatus;
	}

	for(int i = 0; i < dvcsize; i++)	
		statsize += strlen(dvcs[i]);
	
	blkstatus = malloc(statsize /* length of all device strings*/
		+ (ADD == stat ? strlen(alab) : strlen(rlab)) /* length of Added/Removed label */
		+ (dvcsize - 1) * strlen(sep) /* length of separator * number of devices - 1 
											(no separator for single device change) */
		+ 1); /* EOL */
	if(!blkstatus)
		return NULL;

	blkstatus[0] = '\0';

	if(ADD == stat)
		strcat(blkstatus, alab); 
	else
		strcat(blkstatus, rlab);

	for(int i = 0; i < dvcsize; i++)
	{
		strcat(blkstatus, dvcs[i]);
		if(dvcsize > 1 && i != dvcsize - 1)
			strcat(blkstatus, sep);
	}

	return blkstatus;
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
	
	if(timtm == NULL)
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
	char *ret = strstr(name, "\n");
	if(ret != NULL)
		strncpy(ret, "\0", 1);
	return name;
}

char*
gettemperature(char *base, char *sensorNameFile, char *sensorTempFile)
{
	char *sensorName, *sensorTemp, *tempfmt;

	sensorName = readfile(base, sensorNameFile);
	sensorTemp = readfile(base, sensorTempFile);

	if(sensorName == NULL || sensorTemp == NULL)
		return smprintf("");
	tempfmt = smprintf("%s: %02.0f°C", strip(sensorName), atof(sensorTemp) / 1000);
	free(sensorName);
	free(sensorTemp);

	return tempfmt;
}

bool
testexcl(char *str)
{
	bool isincl = true;

	for(int i = strlen(str); i >= 0; --i)
	{
		for(int j = 0; j < LEN; j++)
		{
			if(str[i] == filefmtexcl[j])
			{
				isincl = false;
				break;
			}
		}

		if(!isincl)
			break;
	}

	return isincl;
}

bool
testincl(char *str)
{
	return strstr(str, filefmtincl) != NULL;
}

int
readdsize(char *base)
{
	struct dirent *dc;	
	DIR *dir = opendir(base);
	int size = 0;

	if(dir != NULL)
	{
		while((dc = readdir(dir)) != NULL)
		{
			if(testincl(dc -> d_name) && testexcl(dc -> d_name))
				++size;	
		}

		closedir(dir);
	}
	else
	{
		closedir(dir);
		return -1;
	}
	
	return size;
}

char**
readd(char *base)
{
	char **files;
	struct dirent *dc;	
	DIR *dir = NULL;
	int size = readdsize(basedir);

	if(size < 1)
		return NULL;

	files = malloc(size * sizeof(char*));
	if(!files)
		return NULL;
	
	dir = opendir(base);

	int i = 0;

	while((dc = readdir(dir)) != NULL)
	{
		if(testincl(dc -> d_name) && testexcl(dc -> d_name))
		{
			size_t namesize = strlen(dc -> d_name);
			files[i] = malloc(++namesize);
			if(!files[i])
				continue;
			strcpy(files[i++], dc -> d_name);
		}
	}

	closedir(dir);

	if(i > size)
		return NULL;

	return files;
}

char**
compfiles(int size1, char **files1, int size2, char **files2)
{
	int diffsize = size1 - size2;
	int diffpos = 0;

	char **filediff = malloc(diffsize);
	if(!filediff)
		return NULL;

	for(int i = 0; i < size1; i++)
	{
		bool issame = false;

		for(int j = 0; j < size2; j++)
		{
			if(!strcmp(files1[i], files2[j]))	
			{
				issame = true;
				break;
			}
		}

		if(!issame)
		{
			size_t namesize = strlen(files1[i]);
			filediff[i] = malloc(++namesize);
			if(!filediff[i])
				return NULL;
			strcpy(filediff[diffpos++], files1[i]);
			if(diffpos == diffsize)
				break;
		}
	}

	return filediff;
}

int
main(void)
{
	char *status;
	char *tmbln;
	char *tpac, *tc1, *tc2, *tc3, *tc4;
	int origdsize, newdsize;
	char **origfiles, **newfiles;
	char **addedfiles, **removedfiles;
	char *blkstatus;
	blkstatt blkdevstat;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	origdsize = readdsize(basedir);
	origfiles = readd(basedir);

	for (;;sleep(1)) { 
		tpac = gettemperature(hwmon, "temp1_label", "temp1_input");
		tc1 = gettemperature(hwmon, "temp2_label", "temp2_input");
		tc2 = gettemperature(hwmon, "temp3_label", "temp3_input");
		tc3 = gettemperature(hwmon, "temp4_label", "temp4_input");
		tc4 = gettemperature(hwmon, "temp5_label", "temp5_input");

		tmbln = mktimes("%a %d %b %H:%M %Z %Y", tzlondon);
		
		newdsize = readdsize(basedir);
		newfiles = readd(basedir);

		if(newdsize > origdsize)
		{
			addedfiles = compfiles(newdsize, newfiles, origdsize, origfiles); //added
			blkstatus = blkprintf(newdsize - origdsize, addedfiles, ADD);
			blkdevstat = ADD;	
		}
		else if(origdsize > newdsize)
		{
			removedfiles = compfiles(origdsize, origfiles, newdsize, newfiles); //removed
			blkstatus = blkprintf(origdsize - newdsize, removedfiles, REMOVE);
			blkdevstat = REMOVE;
		}
		else
		{
			blkstatus = blkprintf(0, NULL, NOTHING);
			blkdevstat = NOTHING;
		}

		status = smprintf("CPU temp: %s | %s | %s | %s | %s | Block device changes: %s | Time: %s",
			tpac, tc1, tc2, tc3, tc4, blkstatus, tmbln);
		setstatus(status);

		free(tpac);
		free(tc1);
		free(tc2);
		free(tc3);
		free(tc4);
		free(tmbln);
		free(status);
		free(blkstatus);

		for(int i = 0; i < newdsize; i++)
			free(newfiles[i]);
		free(newfiles);

		switch(blkdevstat)
		{
			case ADD:
				for(int i = 0; i < newdsize - origdsize; i++)
					free(addedfiles[i]);
				free(addedfiles);
				break;
			case REMOVE:
				for(int i = 0; i < origdsize - newdsize; i++)
					free(removedfiles[i]);
				free(removedfiles);
				break;
			case NOTHING:
				//yeah, nothing
				break;
		}
	}

	for(int i = 0; i < origdsize; i++)
		free(origfiles[i]);
	free(origfiles);

	XCloseDisplay(dpy);

	return 0;
}

