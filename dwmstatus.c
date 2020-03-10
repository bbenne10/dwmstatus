#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <curl/curl.h>
#include <gio/gio.h>
#include <glib.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <unistd.h>

#define NM_UNREAD_QUERY "tag:inbox and tag:unread and not tag:archived"

#define BATT_PATH "/sys/class/power_supply/BAT0"

#define MEDIA_PLAYER_BUS_NAME "org.mpris.MediaPlayer2.spotifyd"

#define WEATHER_CHECK_INT 1800
#define WEATHER_CHECK_LOC "30032"
#define WEATHER_CHECK_METRIC 0

static Display *dpy;
static int numCores;
static int checkMedia = 1;
static int checkBatt = 1;
static regex_t weatherRe;

struct MemoryStruct {
  char * memory;
  size_t size;
};

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
    free(path);
    return NULL;
  }
  free(path);

  if (fgets(line, sizeof(line)-1, fd) == NULL) {
    fclose(fd);
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

static size_t
weatherWriteCB(void *contents, size_t size, size_t nmemb, void * userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */
    fprintf(stderr, "not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

char *
getMedia () {
  GError * error = NULL;
  GDBusConnection * bus;
  GVariant * result;
  GVariant * props;
  gchar * * artists = NULL;
  gchar * artist = NULL;
  gchar * title = NULL;

  bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

  if (!bus) {
    if (error) {
      fprintf(stderr, "%s\n", error->message);
      g_error_free(error);
    }
    checkMedia = 0;
    return "";
  }

  result = g_dbus_connection_call_sync(
    bus,
    MEDIA_PLAYER_BUS_NAME,
    "/org/mpris/MediaPlayer2",
    "org.freedesktop.DBus.Properties",
    "Get",
    g_variant_new(
      "(ss)",
      "org.mpris.MediaPlayer2.Player",
      "Metadata"),
    G_VARIANT_TYPE("(v)"),
    G_DBUS_CALL_FLAGS_NONE,
    -1,
    NULL,
    &error);

  if (!result) {
    if (error) {
      fprintf(stderr, "%s\n", error->message);
      g_error_free(error);
    }
    checkMedia = 0;
    return "";
  }

  g_variant_get(result, "(v)", &props);
  g_variant_lookup(props, "xesam:artist", "^a&s", &artists);
  g_variant_lookup(props, "xesam:title", "s", &title);

  if (artists) {
    artist = g_strjoinv(", ", artists);
  } else {
    artist = g_strdup("(UNKNOWN)");
  }

  if (!title) {
    title = g_strdup("UNKNOWN");
  }

  return smprintf("<span color='#00ff00'>&#xf3b6;</span> %s - %s ", artist, title);
}

char *
getWeather() {
  char weatherURI[128];
  struct MemoryStruct chunk;
  CURL * curl = curl_easy_init();
  curl_global_init(CURL_GLOBAL_ALL);
  CURLcode res;

  chunk.memory = malloc(1);
  chunk.size = 0;

  snprintf(weatherURI, sizeof(weatherURI),
           "http://rss.accuweather.com/rss/liveweather_rss.asp?metric=%d&locCode=%s",
           WEATHER_CHECK_METRIC, WEATHER_CHECK_LOC);

  curl_easy_setopt(curl, CURLOPT_URL, weatherURI);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, weatherWriteCB);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

  res = curl_easy_perform(curl);

  char * retstr;
  if (res != CURLE_OK) {
    fprintf(stderr, "Failed to perform request to weather URI\n");
    retstr = smprintf("");
  } else {
    regmatch_t pmatch[3];
    if (regexec(&weatherRe, chunk.memory, 3, pmatch, 0) != 0) {
      fprintf(stderr, "Failed to find any matching substrings\n");
      retstr = smprintf("");
    } else {
      char * cond = strndup(&chunk.memory[pmatch[1].rm_so], pmatch[1].rm_eo - pmatch[1].rm_so);
      char * temp = strndup(&chunk.memory[pmatch[2].rm_so], pmatch[2].rm_eo - pmatch[2].rm_so);
      retstr = smprintf("<span color='#cc241d'>&#xf3b6;</span> %s° (%s)  ", temp, cond);
      free(cond);
      free(temp);
    }
  }
  free(chunk.memory);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return retstr;
}

int
main(void) {
  char * status;
  char * sysAvg;
  char * mediaInfo;
  char * batt;
  char * weather;
  int checkCounter = WEATHER_CHECK_INT;
  int reCompRet;

  // TODO: Volume?

  if (!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "dwmstatus: cannot open display.\n");
    return 1;
  }

  numCores = sysconf(_SC_NPROCESSORS_ONLN);
  reCompRet = regcomp(&weatherRe,
                      "<title>Currently: ([a-zA-Z ]+): ([0-9]+)[C,F]</title>",
                      REG_EXTENDED);
  if (reCompRet != 0) {
    fprintf(stderr, "dwmstatus: failed to compile weather regex\n");
    return 1;
  }

  for (;;sleep(5)) {
    sysAvg = getLoadAvg();

    mediaInfo = checkMedia ? getMedia() : smprintf("");
    batt = checkBatt ? getBatt(BATT_PATH) : smprintf("");

    if (checkCounter >= WEATHER_CHECK_INT) {
      checkCounter = 0;
      weather = getWeather();

      // Use checkCounter to reset and check for batt/media again
      checkMedia = 1;
      checkBatt = 1;
    }

    status = smprintf("%s%s%s%s", weather, batt, mediaInfo, sysAvg);
    setStatus(status);
    free(sysAvg);

    free(mediaInfo);
    free(batt);
    free(status);

    checkCounter += 5;
  }
  XCloseDisplay(dpy);
  return 0;
}
