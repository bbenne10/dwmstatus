#define _DEFAULT_SOURCE
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
#include <unistd.h>

#include <X11/Xlib.h>
#include <mpd/client.h>
#include <notmuch.h>

#define NM_DB_PATH "/home/bryan/.mail/work"
#define BATT_PATH "/sys/class/power_supply/BAT0"

static Display *dpy;
static int numCores;
static int checkMail = 1;
static int checkMPD = 1;
static int checkBatt = 1;

char *
smprintf(char *fmt, ...) {
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

char *
readLineFromFile(char *base, char *file) {
  char *path, line[513];
  FILE *fd;

  memset(line, 0, sizeof(line));

  path = smprintf("%s/%s", base, file);
  fd = fopen(path, "r");
  if (fd == NULL) {
    return NULL;
  }
  free(path);

  if (fgets(line, sizeof(line)-1, fd) == NULL) {
    return NULL;
  }
  fclose(fd);

  return smprintf("%s", line);
}

void
setStatus(char *str) {
  XStoreName(dpy, DefaultRootWindow(dpy), str);
  XSync(dpy, False);
}

char *
getBatt(char *base) {
  char *co;
  int descap, remcap;

  descap = -1;
  remcap = -1;

  co = readLineFromFile(base, "present");
  if (co == NULL || co[0] != '1') {
    if (co != NULL) {
      free(co);
    }
    fprintf(stderr, "No battery found\n");
    checkBatt = 0;
    return smprintf("");
  }
  free(co);

  co = readLineFromFile(base, "charge_full_design");
  if (co == NULL) {
    co = readLineFromFile(base, "energy_full_design");
    if (co == NULL) {
      checkBatt = 0;
      fprintf(stderr, "No battery full file found\n");
      return smprintf("");
    }
  }
  sscanf(co, "%d", &descap);
  free(co);

  co = readLineFromFile(base, "charge_now");
  if (co == NULL) {
    co = readLineFromFile(base, "energy_now");
    if (co == NULL) {
      checkBatt = 0;
      fprintf(stderr, "No battery now file found\n");
      return smprintf("");
    }
  }
  sscanf(co, "%d", &remcap);
  free(co);

  if (remcap < 0 || descap < 0) {
    checkBatt = 0;
    fprintf(stderr, "Invalid battery range found\n");
    return smprintf("");
  }

  float perc = ((float)remcap / (float)descap) * 100;
  char * icon;
  if (perc > 75) {
    icon = "&#xf239;";
  } else if (perc > 50) {
    icon = "&#xf241;";
  } else if (perc > 25) {
    icon = "&#xf242;";
  } else {
    icon = "&#xf243;";
  }

  return smprintf("<span color='#689da6'>%s</span> %.0f%% ",
                  icon, ((float)remcap / (float)descap) * 100);
}

char *
getLoadAvg() {
  double avgs[1];

  if (getloadavg(avgs, 1) < 0) {
    perror("getloadavg");
    exit(1);
  }

  return smprintf("<span color='#b16286'>&#xf0e7;</span> %d%%", (int)((avgs[0] * 100 / numCores)));
}

char *
getMpd() {
  struct mpd_song * song = NULL;
  const char * title = NULL;
  const char * artist = NULL;
  char * retstr = NULL;

  struct mpd_connection * conn ;
  if (!(conn = mpd_connection_new("localhost", 0, 30000)) || mpd_connection_get_error(conn)){
    checkMPD = 0;
    fprintf(stderr, "Could not connect to MPD");
    return smprintf("");
  }

  mpd_command_list_begin(conn, true);
  mpd_send_status(conn);
  mpd_send_current_song(conn);
  mpd_command_list_end(conn);

  struct mpd_status* theStatus = mpd_recv_status(conn);

  if ((theStatus) && (mpd_status_get_state(theStatus) == MPD_STATE_PLAY)) {
    mpd_response_next(conn);
    song = mpd_recv_song(conn);
    title = smprintf("%s",mpd_song_get_tag(song, MPD_TAG_TITLE, 0));
    artist = smprintf("%s",mpd_song_get_tag(song, MPD_TAG_ARTIST, 0));

    mpd_song_free(song);
    retstr = smprintf("<span color='#b8bb26'>&#xf025;</span> %s - %s  ", artist, title);
    free((char*)title);
    free((char*)artist);
  } else {
    retstr = smprintf("");
  }

  mpd_response_finish(conn);
  mpd_connection_free(conn);
  return retstr;
}

char *
getMail() {
  notmuch_database_t *db;
  notmuch_status_t status = notmuch_database_open(
                                                  NM_DB_PATH, NOTMUCH_DATABASE_MODE_READ_ONLY, &db);
  notmuch_query_t * query;
  unsigned int count = 0;
  char * retstr = smprintf("");

  if (status != NOTMUCH_STATUS_SUCCESS) {
    fprintf(stderr, "Failed to open nm database\n");
    checkMail = 0;
  } else {
    query = notmuch_query_create(db, "tag:inbox and tag:unread");
    status = notmuch_query_count_messages_st(query, &count);
    if (status != NOTMUCH_STATUS_SUCCESS) {
      fprintf(stderr, "Failed to issue count query\n");
      checkMail = 0;
    }

    if (count > 0) {
      retstr = smprintf("<span color='#fb4934'>&#xf0e0;</span>  ");
    } else {
      retstr = "";
    }
    notmuch_database_close(db);
  }

  return retstr;
}

int
main(void) {
  char * status;
  char * sysAvg;
  char * mpdStatus;
  char * mail;
  char * batt;

  // TODO: Weather
  // TODO: Volume?

  if (!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "dwmstatus: cannot open display.\n");
    return 1;
  }

  numCores = sysconf(_SC_NPROCESSORS_ONLN);

  for (;;sleep(5)) {
    sysAvg = getLoadAvg();

    if (checkMPD) {
      mpdStatus = getMpd();
    }

    if (checkMail) {
      mail = getMail();
    }

    if (checkBatt) {
      batt = getBatt(BATT_PATH);
    }

    status = smprintf("%s%s%s%s",
                      mail, batt, mpdStatus, sysAvg);
    setStatus(status);
    free(sysAvg);

    if (mpdStatus != NULL && strcmp(mpdStatus, "")) {
      free(mpdStatus);
    }

    if (mail != NULL && strcmp(mail, "")) {
      free(mail);
    }

    if (batt != NULL && strcmp(batt, "")) {
      free(batt);
    }

    free(status);
  }

  XCloseDisplay(dpy);

  return 0;
}
