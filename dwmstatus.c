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

char *sysbat = "/sys/class/power_supply/BAT0/capacity";
char *tzsf = "US/Pacific";
char *tztokyo= "Japan";
char *tzutc = "UTC";
char *tzberlin = "Europe/Berlin";

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

	memset(buf, 0, sizeof(buf));
	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
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
batcap(void)
{
	char cap[16], *r = NULL;
	FILE *f = fopen(sysbat, "r");

	memset(cap, 1, sizeof cap);
	if (f) {
		r = fgets(cap, sizeof cap, f);
		if (r == cap)
			cap[strlen(cap)-1] = '\0';
		fclose(f);
	}
	return smprintf("%s%%", r != NULL ? r : "?");
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		perror("getloadavg");
		exit(1);
	}

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

int
main(void)
{
	char *cap;
	char *status;
	char *avgs;
	char *tmsf;
	char *tmtokyo;
	char *tmutc;
	char *tmbln;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(90)) {
		cap = batcap();
		avgs = loadavg();
		tmsf = mktimes("%H:%M", tzsf);
		tmtokyo = mktimes("%H:%M", tztokyo);
		tmutc = mktimes("%H:%M", tzutc);
		tmbln = mktimes("KW %W %a %d %b %H:%M %Z %Y", tzberlin);

		status = smprintf("B:%s L:%s SF:%s JP:%s U:%s %s",
				cap, avgs, tmsf, tmtokyo, tmutc, tmbln);
		setstatus(status);
		free(avgs);
		free(tmsf);
		free(tmtokyo);
		free(tmutc);
		free(tmbln);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

