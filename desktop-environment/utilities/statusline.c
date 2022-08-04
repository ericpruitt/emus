/**
 * Status Line
 *
 * Emit system status lines once per second that typically include the battery
 * status, day of the week, the day of the month and the time. Supplementary
 * clocks from different time zones may also be displayed. Refer to the "usage"
 * function for more information.
 *
 * The moon phase logic was adapted from release 2.5 of John Walker's "A Moon
 * for the Sun" (AKA "moontool") and Kevin Turner's Python port of the same
 * tool.
 *
 * Make: c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE -o $@ $? -lm
 * Copyright: Eric Pruitt (https://www.codevat.com/)
 * License: BSD 2-Clause License (https://opensource.org/licenses/BSD-2-Clause)
 */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static char *battery_indicator(const char *);
static void delete_range(char *, size_t, size_t);
static size_t dow_with_ordinal_dom(char *, size_t, struct tm *);
static void gmt_to_utc(char *);
static double mtime(const char *);
static size_t load_indicators_from_file(char *, size_t, const char *,
                                        const char *);
static size_t tzstrftime(char *, size_t, const char *, time_t, const char *);

/**
 * Get the number of members in a fixed-length array.
 *
 * Arguments:
 * - x: Array
 *
 * Return: Length of array.
 */
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))

/**
 * Separator used to show strong division between indicators.
 */
#define SEPARATOR " | "

/**
 * Separator used to show soft division between indicators.
 */
#define SOFT_SEPARATOR " - "

/**
 * Identifiers for the named phases of the moon.
 */
typedef enum {
    MOON_PHASE_NEW_MOON,
    MOON_PHASE_WAXING_CRESCENT,
    MOON_PHASE_FIRST_QUARTER,
    MOON_PHASE_WAXING_GIBBOUS,
    MOON_PHASE_FULL_MOON,
    MOON_PHASE_WANING_GIBBOUS,
    MOON_PHASE_LAST_QUARTER,
    MOON_PHASE_WANING_CRESCENT,
    MOON_PHASE_COUNT,
} moon_phase_et;

/**
 * Works like _strftime(3)_ but expects a `time_t` timestamp instead of a
 * `struct tm *` and accepts an additional parameter, the time zone in which
 * the conversion should take place.
 *
 * Arguments:
 * - dest: The result will written to this character array.
 * - sizeofdest: Size of the result buffer.
 * - format: Format string as defined for _strftime(3)_.
 * - when: Unix timestamp representing the time to be formatted.
 * - where: Time zone in which the conversion takes place e.g.
 *   "America/Las_Angeles".
 *
 * Return: Number of bytes written to "dest" excluding the null byte.
 */
static size_t tzstrftime(char *dest, size_t sizeofdest, const char *format,
  time_t when, const char *where)
{
    char original_tz_value[1024];
    const struct tm *timespec;
    const char *tz;

    size_t rval = 0;
    int saved_errno = 0;

    if ((strlen(where) + 1) > sizeof(original_tz_value)) {
        errno = EOVERFLOW;
        return 0;
    }

    if ((tz = getenv("TZ"))) {
        strncpy(original_tz_value, tz, sizeof(original_tz_value));

        if (original_tz_value[sizeof(original_tz_value) - 1] != '\0') {
            errno = EOVERFLOW;
            return 0;
        }
    }

    if (setenv("TZ", where, 1)) {
        return 0;
    }

    timespec = localtime(&when);
    saved_errno = errno;

    if (timespec) {
        if ((rval = strftime(dest, sizeofdest, format, timespec))) {
            gmt_to_utc(dest);
        } else {
            saved_errno = errno;
        }
    }

    if (tz) {
        setenv("TZ", original_tz_value, 1);
    } else {
        unsetenv("TZ");
    }

    tzset();
    errno = saved_errno;
    return rval;
}

/**
 * Write the day of the week and an ordinal day of the month to "dest" e.g.
 * "Wed. the 21st."
 *
 * Arguments:
 * - dest: The string will be written to this character array.
 * - sizeofdest: Size of "dest".
 * - tm: Time structure representing the date to be displayed.
 *
 * Return: Number of bytes written to "dest" not including the null byte.
 */
static size_t dow_with_ordinal_dom(char *dest, size_t sizeofdest,
  struct tm *tm)
{
    size_t k;
    char *cursor = dest;

    // The "xx" reserve spaces for the ordinal indicator so strftime will fail
    // if the destination array is too small.
    if (!(k = strftime(dest, sizeofdest, "%a. the %dxx", tm))) {
        return 0;
    }

    cursor += k - 2;

    // Remove leading 0 from day of month. Some strftime implementations
    // support "%-d" to achieve this, but it is not part of the ISO C99
    // standard.
    if (tm->tm_mday < 10) {
        *(cursor - 2) = *(cursor - 1);
        cursor--;
    }

    if (tm->tm_mday == 11 || tm->tm_mday == 12 || tm->tm_mday == 13) {
        cursor = stpcpy(cursor, "th");
    } else {
        switch (tm->tm_mday % 10) {
          case 1:
            cursor = stpcpy(cursor, "st");
            break;
          case 2:
            cursor = stpcpy(cursor, "nd");
            break;
          case 3:
            cursor = stpcpy(cursor, "rd");
            break;
          default:
            cursor = stpcpy(cursor, "th");
            break;
        }
    }

    return (size_t) (cursor - dest);
}

/**
 * Return a string representing the state of the battery. There are several
 * different states states (the XX below represents the charge percentage):
 *
 * - `⚡-`: Shown when the file containing the battery data could not be opened.
 * - `⚡↑XX`: Shown when the battery is charging.
 * - `⚡↓XX`: Shown when the battery is draining.
 * - `⚡XX`: Shown when the battery is not draining or charging.
 * - `⚡?`: Shown when the file containing the battery data could not be parsed
 *   because the format was not recognized.
 * - `⚡!`: Shown when there was an error reading the battery data file.
 *
 * Arguments:
 * - path: Path to the file containing the battery data in the form of a uevent
 *   sysfs battery node e.g. "/sys/class/power_supply/BAT0/uevent".
 *
 * Return: A pointer to a statically allocated array containing the indicator
 * text.
 */
static char *battery_indicator(const char *path)
{
    char *endptr;
    FILE *file;
    long int long_int;
    char *match;

    size_t bufsize = 0;
    int capacity_percent = -1;
    static char icon[16] = "";
    char *line = NULL;
    signed char trend = 0;

    while (!(file = fopen(path, "r"))) {
        if (errno != EINTR) {
            return strcpy(icon, "⚡-");
        }
    }

    while (getline(&line, &bufsize, file) != -1) {
        if ((match = strstr(line, "POWER_SUPPLY_CAPACITY="))) {
            match += sizeof("POWER_SUPPLY_CAPACITY=") - 1;
            errno = 0;
            long_int = strtol(match, &endptr, 10);

            if (!errno && endptr != match && long_int >= 0 && long_int < 101) {
                capacity_percent = (int) long_int;
            }
        } else if (strstr(line, "POWER_SUPPLY_STATUS=Charging")) {
            trend = +1;
        } else if (strstr(line, "POWER_SUPPLY_STATUS=Discharging")) {
            trend = -1;
        }
    }

    free(line);

    if (capacity_percent == -1) {
        if (feof(file)) {
            strcpy(icon, "⚡?");
        } else {
            strcpy(icon, "⚡!");
        }
    } else if (trend > 0 && capacity_percent < 100) {
        snprintf(icon, sizeof(icon), "⚡↑%d", capacity_percent);
    } else if (trend < 0) {
        snprintf(icon, sizeof(icon), "⚡↓%d", capacity_percent);
    } else {
        snprintf(icon, sizeof(icon), "⚡%d", capacity_percent);
    }

    fclose(file);
    return icon;
}

/**
 * Return file's modification time.
 *
 * Arguments:
 * - path: File path.
 *
 * Return: -1 on error and the file's modification time otherwise.
 */
static double mtime(const char *path)
{
    struct stat status;

    if (stat(path, &status)) {
        return -1;
    }

    // When st_mtime is a macro, it probably means the OS supports sub-second
    // filesystem timestamps.
    #ifdef st_mtime
    return status.st_mtim.tv_sec + status.st_mtim.tv_nsec / 1E9;
    #else
    return (double) status.st_mtime;
    #endif
}

/**
 * Reads lines from a file treating each one as a separate status bar indicator
 * and writes them to "dest".
 *
 * Arguments:
 * - dest: Output destination.
 * - sizeofdest: The size of the destination buffer.
 * - path File path.
 * - sep: Separator that will be added between indicators. Can be null to have
 *   no separation.
 *
 * Return: Number of characters written to "dest" not including the null.
 */
static size_t load_indicators_from_file(char *dest, size_t sizeofdest,
  const char *path, const char *sep)
{
    FILE *file;
    ssize_t line_length;

    size_t bufsize = 0;
    char *cursor = dest;
    char *line = NULL;
    int saved_errno = 0;
    size_t seplen = strlen(sep);

    while (!(file = fopen(path, "r"))) {
        if (errno != EINTR) {
            return 0;
        }
    }

    while ((line_length = getline(&line, &bufsize, file)) != -1) {
        if (line[line_length - 1] == '\n') {
            line[line_length - 1] = '\0';
        }

        // This is not an off-by-one-error: line_length was previously the
        // length of the line plus the newline, but now it represents the
        // length of the indicator plus the null byte.
        if (((size_t) line_length + seplen) > sizeofdest) {
            errno = EFBIG;
            break;
        } else if (line[0] != '\0') {
            cursor = stpcpy(cursor, line);

            if (sep) {
                cursor = stpcpy(cursor, sep);
            }
        }
    }

    free(line);

    if (!feof(file)) {
        saved_errno = errno;
    }

    fclose(file);
    errno = saved_errno;
    return (size_t) (cursor - dest);
}

/**
 * Delete range of characters from string.
 *
 * Arguments:
 * - text: String to modify.
 * - start: Offset of the beginning of the range to delete.
 * - count: Number of characters to delete.
 */
static void delete_range(char *text, size_t start, size_t count)
{
    char *read = text + start + count;
    char *write = text + start;

    if (read != write) {
        while ((*write++ = *read++));
    }
}

/**
 * Convert degrees to radians.
 *
 * Arguments:
 * - radians
 *
 * Return: Angle in degrees.
 */
static inline double to_radians(double degrees)
{
    return degrees * M_PI / 180;
}

/**
 * Convert radians to degrees.
 *
 * Arguments:
 * - degrees
 *
 * Return: Angle in radians.
 */
static inline double to_degrees(double radians)
{
    return radians * 180 / M_PI;
}

/**
 * Bound an angle in degrees to the interval [0, 360).
 *
 * Arguments:
 * - degrees
 *
 * Return: An angle greater than or equal to 0 and less than 360.
 */
static inline double bound_angle(double degrees)
{
    return degrees - 360 * floor(degrees / 360);
}

/**
 * Solve Kepler's equation for the true anomaly given the mean anomaly in
 * degrees and the eccentricity of the orbit.
 *
 * Arguments:
 * - mean_anomaly
 * - eccentricity
 *
 * Return: True anomaly in radians.
 */
static inline double kepler(double mean_anomaly, double eccentricity)
{
    double delta;

    double m = to_radians(mean_anomaly);
    double e = m;

    while (1) {
        delta = e - eccentricity * sin(e) - m;
        e = e - delta / (1 - eccentricity * cos(e));

        if ((delta >= -1e-6) && (delta <= 1e-6)) {
            return e;
        }
    }
}

/**
 * Compute the moon phase.
 *
 * Arguments:
 * - when: UNIX timestamp representing the time.
 *
 * Return: A number on the interval [0, 1.0) where 0 represents a new moon and
 * 0.5 represents a full moon.
 */
static double moon_phase(time_t when)
{
    static const double earth_eccentricity = 0.016718;
    static const double ecliptic_longitude_epoch = 278.833540;
    static const double ecliptic_longitude_perigee = 282.596403;
    static const double moon_mean_longitude_epoch = 64.975464;
    static const double moon_mean_perigee_epoch = 349.383063;

    // Days since 1979-12-31T00:00:00Z. In John Walker's moontool.c, the epoch
    // is commented as "1980 January 0.0." I'm not sure why January 0 is used
    // instead of December 31, but the value of "day" computed here for a given
    // timestamp is the same as what the "phase" function in moontool.c
    // computes for Day via "jtime(gmtime(&t)) - epoch /* 2444238.5 */", so I
    // think the math is still correct. On my Debian 10 system with
    // python-egenix-mxdatetime version 3.2.9-1, the result of Kevin Turner's
    // calculation of this value differs slightly from Walker's. For some
    // reason, mx.DateTime.jdn is 7 hours behind Walker's value at the time of
    // this writing (2021-10-14 US/Pacific) which happens to be the UTC offset
    // of my time zone, so I think there is a bug in the mx library or in
    // Turner's usage of it.
    double day = (double) when / 86400 - 3651;

    // Solar position calculations
    double N = bound_angle(day * 360 / 365.2422);
    double M = bound_angle(N + ecliptic_longitude_epoch -
        ecliptic_longitude_perigee);
    double Ec = 360 / M_PI * atan(tan(kepler(M, earth_eccentricity)/ 2) *
        sqrt((1 + earth_eccentricity) / (1 - earth_eccentricity)));
    double lambda_sun = bound_angle(Ec + ecliptic_longitude_perigee);

    // Lunar position calculations
    double moon_longitude = bound_angle(13.1763966 * day +
        moon_mean_longitude_epoch);
    double MM = bound_angle(moon_longitude - 0.1114041 * day -
        moon_mean_perigee_epoch);
    double evection = sin(to_radians(2 * (moon_longitude - lambda_sun) - MM)) *
        1.2739;
    double annual_eq = 0.1858 * sin(to_radians(M));
    double A3 = 0.37 * sin(to_radians(M));
    double MmP = MM + evection - annual_eq - A3;
    double mEc = 6.2886 * sin(to_radians(MmP));
    double A4 = 0.214 * sin(to_radians(2 * MmP));
    double lP = moon_longitude + evection + mEc - annual_eq + A4;
    double variation = 0.6583 * sin(to_radians(2 * (lP - lambda_sun)));
    double lPP = lP + variation;

    // Final phase calculation
    return bound_angle(lPP - lambda_sun) / 360;
}

/**
 * Return a string indicating the phase of the moon. Within 12 hours of a new
 * moon or full moon, a "⁺" is prepended to the icon if the moon is waxing, and
 * a "⁻" if the moon is waning.
 *
 * Arguments:
 * - when: UNIX timestamp representing the time.
 * - southern_hemisphere: When this is 0, the icon fills right-to-left as the
 *   moon does observed from most of the northern hemisphere. For any non-zero
 *   value, the icon fills left-to-right as the moon does observed most from
 *   the southern hemisphere. The southern hemisphere rendering may be
 *   inaccurate for some fonts. See
 *   <https://www.unicode.org/L2/L2017/17304-moon-var.pdf> for details.
 * - invert: When this is non-zero, the light and dark side of the moon are
 *   inverted. This is useful when the foreground and background colors used to
 *   display monochrome moon phase icons produce unintuitive pictures when
 *   using the correct characters.
 *
 * Return: An icon representing the current moon phase.
 */
const char *moon_icon(time_t when, int southern_hemisphere, int invert)
{
    static const char *icons[MOON_PHASE_COUNT] = {
        [MOON_PHASE_NEW_MOON] =        "🌑",
        [MOON_PHASE_WAXING_CRESCENT] = "🌒",
        [MOON_PHASE_FIRST_QUARTER] =   "🌓",
        [MOON_PHASE_WAXING_GIBBOUS] =  "🌔",
        [MOON_PHASE_FULL_MOON] =       "🌕",
        [MOON_PHASE_WANING_GIBBOUS] =  "🌖",
        [MOON_PHASE_LAST_QUARTER] =    "🌗",
        [MOON_PHASE_WANING_CRESCENT] = "🌘",
    };

    static char text[8] = "";

    double phase = moon_phase(when);
    int icon = (int) (phase * 8 + 0.5) % 8;

    double t_minus_12h = moon_phase(when - 43200);
    double t_plus_12h = moon_phase(when + 43200);

    if ((phase <= 0.5 && 0.5 <= t_plus_12h) || t_minus_12h > phase) {
        strcpy(text, "⁺");
    } else if ((t_minus_12h <= 0.5 && 0.5 <= phase) || t_plus_12h < phase) {
        strcpy(text, "⁻");
    } else {
        text[0] = '\0';
    }

    if (invert) {
        // Treat the new moon icon as the full moon icon and vice versa.
        icon = (icon + 4) % 8;
    }

    // When viewed from the southern hemisphere, the moon fills left-to-right
    // instead of right-to-left.
    if (southern_hemisphere) {
        icon = (8 - icon) % 8;
    }

    return strcat(text, icons[icon]);
}

/**
 * Round the time down to 00:00:00 local time if it does not already represent
 * that time.
 *
 * Arguments:
 * - when: UNIX timestamp.
 *
 * Return: UNIX timestamp representation of midnight.
 */
static time_t round_down_to_midnight(time_t when)
{
    char buf[20];
    struct tm *in;
    struct tm out;

    when -= when % 60;
    in = localtime(&when);

    if (in->tm_hour == 0 && in->tm_min == 0) {
        return when;
    }

    sprintf(buf, "%d-%02d-%02d 00:00:00", in->tm_year + 1900, in->tm_mon + 1,
        in->tm_mday);
    strptime(buf, "%Y-%m-%d %H:%M:%S", &out);
    return mktime(&out);
}

/**
 * Compute the time of sunrise and sunset for a particular location rounded
 * down to the nearest minute. The formulas come from [_General Solar Position
 * Calculations_ by the NOAA Global Monitoring
 * Division](https://gml.noaa.gov/grad/solcalc/solareqns.PDF).
 *
 * Arguments:
 * - when: A UNIX timestamp representing the day for which to compute the
 *   sunset and sunrise.
 * - latitude: The latitude of the location in degrees.
 * - longitude: The longitude of the location in degrees.
 * - sunrise: Location at which the computed sunrise time is stored. This can
 *   be NULL if the caller does not want the value.
 * - sunset: Location at which the computed sunset time is stored. This can be
 *   NULL if the caller does not want the value.
 */
static void sunrise_sunset_times(time_t when, double latitude,
  double longitude, time_t *sunrise, time_t *sunset)
{
    double time;

    // For the special case of sunrise or sunset, the zenith is set to 90.833°
    // (the approximate correction for atmospheric refraction at sunrise and
    // sunset, and the size of the solar disk).
    const double zenith = cos(to_radians(90.833));

    int days_in_year = 365;
    double latrads = to_radians(latitude);
    time_t midnight = when - when % 86400;
    struct tm *timespec = gmtime(&when);

    int year = timespec->tm_year + 1900;

    int is_leap_year = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);

    double fractional_year = 2 * M_PI / (days_in_year + is_leap_year) * (
        timespec->tm_yday + (timespec->tm_hour - 12) / 24
    );

    double decl = 0.006918 - 0.399912 * cos(fractional_year) + 0.070257 *
        sin(fractional_year) - 0.006758 * cos(2 * fractional_year) + 0.000907 *
        sin(2 * fractional_year) - 0.002697 * cos(3 * fractional_year) +
        0.00148 * sin(3 * fractional_year);
    double eqtime = 229.18 * (0.000075 + 0.001868 * cos(fractional_year) -
        0.032077 * sin(fractional_year) - 0.014615 * cos(2 * fractional_year) -
        0.040849 * sin(2 * fractional_year));

    double hour_angle = to_degrees(
        acos(zenith / (cos(latrads) * cos(decl)) - tan(latrads) * tan(decl))
    );

    if (sunrise) {
        time = midnight + 60 * (720 - 4 * (longitude + hour_angle) - eqtime);
        *sunrise = (time_t) (time - fmod(time, 60));
    }

    if (sunset) {
        time = midnight + 60 * (720 - 4 * (longitude - hour_angle) - eqtime);
        *sunset = (time_t) (time - fmod(time, 60));
    }
}

/**
 * Return a string showing when the next sunrise or sunset takes place.
 *
 * Arguments:
 * - when: A UNIX timestamp representing the day for which to compute the
 *   sunset and sunrise.
 * - latitude: The latitude of the location in degrees.
 * - longitude: The longitude of the location in degrees.
 * - sunrise: Location at which the computed sunrise time is stored. This can
 *   be NULL if the caller does not want the value.
 * - sunset: Location at which the computed sunset time is stored. This can be
 *   NULL if the caller does not want the value.
 */
static char *sunrise_sunset_info(time_t when, double latitude,
  double longitude)
{
    const char *format;
    time_t midnight;
    time_t sunrise;
    time_t sunset;
    static char text[64];
    time_t timestamp;

    const char sunrise_format[] = "🌅 %R";
    const char sunset_format[] = "🌙 %R";

    midnight = round_down_to_midnight(when);
    sunrise_sunset_times(midnight, latitude, longitude, &sunrise, &sunset);

    if (sunset <= when) {
        // It is already past sunset for the current day, so figure out when
        // the next sunrise is. 36 hours in the future is guaranteed to be
        // exactly one calendar day ahead barring the introduction of some
        // dramatic changes in time zones.
        midnight = round_down_to_midnight(midnight + 36 * 3600);
        sunrise_sunset_times(midnight, latitude, longitude, &sunrise, NULL);

        timestamp = sunrise;
        format = sunrise_format;
    } else if (sunrise > when) {
        timestamp = sunrise;
        format = sunrise_format;
    } else {
        timestamp = sunset;
        format = sunset_format;
    }

    if (!strftime(text, sizeof(text), format, localtime(&timestamp))) {
        strncpy(text, "⚠️", sizeof(text));
    }

    return text;
}

/**
 * Parse the latitude and longitude from a string.
 *
 * Arguments:
 * - text: String that should contain the latitude and longitude separated by a
 *   comma.
 * - latitude: Output pointer for the latitude.
 * - longitude: Output pointer for the longitude.
 *
 * Return: 0 if the coordinates were valid and -1 otherwise.
 */
static int strtolatlong(const char *text, double *latitude, double *longitude)
{
    char coordinates[128];
    char *cursor;
    char *endptr;
    char *latstr;
    char *suffix;

    int latsuffix = 0;
    int longsuffix = 0;
    char *longstr = NULL;

    if (strlen(text) + 1 > sizeof(coordinates)) {
        errno = EMSGSIZE;
        return -1;
    }

    strcpy(coordinates, text);
    latstr = coordinates;

    for (cursor = coordinates; *cursor; cursor++) {
        if (*cursor == ',') {
            // Input contains more than one comma.
            if (longstr) {
                errno = EINVAL;
                return -1;
            }

            *cursor = '\0';
            longstr = cursor + 1;
        }
    }

    // Input contains no commas.
    if (!longstr) {
        errno = EINVAL;
        return -1;
    }

    suffix = longstr - 2;

    if ((*suffix > '9' || *suffix < '0') && *suffix != '.') {
        switch (*suffix) {
          case 'S':
          case 's':
            latsuffix = -1;

          case 'N':
          case 'n':
            latsuffix = latsuffix ? latsuffix : 1;
            *suffix = '\0';
            break;

          default:
            errno = EINVAL;
            return -1;
        }
    }

    suffix = longstr + strlen(longstr) - 1;

    if ((*suffix > '9' || *suffix < '0') && *suffix != '.') {
        switch (*suffix) {
          case 'W':
          case 'w':
            longsuffix = -1;

          case 'E':
          case 'e':
            longsuffix = longsuffix ? longsuffix : 1;
            *suffix = '\0';
            break;

          default:
            errno = EINVAL;
            return -1;
        }
    }

    // Either both values must have a suffix, or both must lack a suffix.
    if ((!latsuffix && longsuffix) || (latsuffix && !longsuffix)) {
        errno = EINVAL;
        return -1;
    }

    *latitude = strtod(latstr, &endptr);

    if (errno || endptr == latstr) {
        errno = EINVAL;
        return -1;
    } else if (*latitude < -90 || *latitude > 90) {
        errno = EDOM;
        return -1;
    } else if (latsuffix) {
        // When an "N" or "S" suffix is specified, negative numbers are not
        // allowed.
        if (signbit(*latitude)) {
            errno = EINVAL;
            return -1;
        }

        *latitude *= latsuffix;
    }

    *longitude = strtod(longstr, &endptr);

    if (errno || endptr == longstr) {
        errno = EINVAL;
        return -1;
    } else if (*longitude < -180 || *longitude > 180) {
        errno = EDOM;
        return -1;
    } else if (longsuffix) {
        // When a "W" or "E" suffix is specified, negative numbers are not
        // allowed.
        if (signbit(*longitude)) {
            errno = EINVAL;
            return -1;
        }

        *longitude *= longsuffix;
    }

    // The NOAA formulas seem to break down at the poles. The web application
    // at https://gml.noaa.gov/grad/solcalc/ clamps latitude values to +/-89.9,
    // but I ran into problems with that value, so I picked stricter limits.
    if (*latitude < -89) {
        *latitude = -89;
    } else if (*latitude > 89) {
        *latitude = 89;
    }

    return 0;
}

/**
 * Replace all occurrences of "GMT" with "UTC".
 *
 * Arguments:
 * - s: String to modify.
 */
static void gmt_to_utc(char *s)
{
    for (; (s = strstr(s, "GMT")); *s++ = 'U', *s++ = 'T', *s++ = 'C');
}

/**
 * Display application usage information.
 *
 * Arguments:
 * - self: Name or path of compiled executable.
 */
static void usage(const char *self)
{
    printf(
        "Usage: %s [-1] [-b PATH] [-c COORDINATES] [-Mmn] [-s PATH] [-f] [-z TIMEZONE]...\n"
        "\n"
        "Updates the X11 root window name once per second. It displays the "
        "battery\nstatus, day of the week, day of the month and can also "
        "display several\nsupplementary clocks in different time zones. Any "
        "occurrences of \"GMT\" are\nreplaced with \"UTC\" before displaying "
        "the clocks. This is not configurable.\n"
        "\n"
        "Exit statuses:\n"
        "  1        Fatal error encountered.\n"
        "\n"
        "Options:\n"
        "  -1       Print one status line and exit without setting the X11\n"
        "           root window name.\n"
        "  -b PATH  Path to uevent battery data. When unset, this defaults\n"
        "           \"/sys/class/power_supply/BAT0/uevent\" if it that path\n"
        "           can be read during program initialization.\n"
        "  -c COORDINATES\n"
        "           Show sunrise and sunset times for the given longitude\n"
        "           and latitude which are specified as two numbers\n"
        "           separated by a comma. This option does NOT support\n"
        "           extreme latitudes correctly and will likely fail in\n"
        "           areas experiencing midnight sun or polar sun. To unset\n"
        "           previously defined coordinates, specify \"-\" as the\n"
        "           coordinates.\n"
        "  -f       Force setting the X11 root window name. Without this\n"
        "           flag, the status bar will only be printed on stdout when\n"
        "           stdout is a TTY.\n"
        "  -h       Show this text and exit.\n"
        "  -i       Invert the light and dark side of the moon. This is useful\n"
        "           when the foreground and background colors used to display\n"
        "           monochrome moon phase icons produce unintuitive pictures\n"
        "           when using the correct characters.\n"
        "  -M       Display the current phase of the moon as it would appear\n"
        "           in the southern hemisphere.\n"
        "  -m       Display the current phase of the moon as it would appear\n"
        "           in the northern hemisphere.\n"
        "  -n       Force dry run; do not set the X11 root window name even\n"
        "           if stdout is not a TTY.\n"
        "  -s PATH  Load status bar indicators from this file. Each line is\n"
        "           treated as a separate indicator. It is best to host this\n"
        "           this file on a fast filesystem (tmpfs, ramfs, etc.) to\n"
        "           reduce the likelihood of disk latency slowing down the\n"
        "           clock. The file is only re-read when the mtime changes.\n"
        "           Any updates to this file should be done in an atomic\n"
        "           manner i.e. rename(2) on most Unix filesystems. If the\n"
        "           size of the file exceeds approximately 1KiB, text may be\n"
        "           discarded or truncated.\n"
        "  -z TIMEZONE\n"
        "           Display a supplementary clock for the given time zone.\n"
        "           This flag can be specified multiple times to show\n"
        "           multiple clocks, but a clock will only be shown when\n"
        "           either its time of day or time zone name / abbreviation\n"
        "           differ from the local time's. This allows the user to\n"
        "           define multiple clocks that only appear when needed i.e.\n"
        "           after changing the host's local time while traveling.\n"
        "           Clocks are shown in the order they appear on the command\n"
        "           line followed by the default clock. When different time\n"
        "           zones would result in duplicate clocks, only the first\n"
        "           one is shown. If this option is only specified once and\n"
        "           its value is \"XXX\", only the default clock for the\n"
        "           local time zone is shown, but some internal changes are\n"
        "           made to address a bug documented below.\n"
        "\n"
        "Bugs:\n"
        "  On Linux with glibc, changes to the system's default time zone\n"
        "  are reflected in calls to tzset(3) immediately, but this is not\n"
        "  the case for OpenBSD: its implementation is somewhat lazy and\n"
        "  will not do any further processing if the TZ environment variable\n"
        "  has the same value it did when tzset(3) was previously called.\n"
        "  Using \"-z\" internally changes TZ and calls tzset(3), but if the\n"
        "  user only wants to display one clock, the reserved value \"XXX\"\n"
        "  can be used as documented above to work around this issue on\n"
        "  OpenBSD and any other platforms that behave similarly.\n"
        ,
        self
    );
}

int main(int argc, char **argv)
{
    char altclock[64];
    const char *altzones[8];
    char *clocks;
    size_t k;
    char localclock[64];
    char message[2048];
    int multiple_clocks;
    time_t now;
    struct tm nowtm;
    int option;
    struct tm *ptm;
    int saved_errno;
    double status_file_mt_now;
    struct timeval tv;

    size_t altzones_count = 0;
    const char *battery_data_path = "/sys/class/power_supply/BAT0/uevent";
    int battery_data_path_explicit = 0;
    int first = 1;
    char indicators_from_file[1024] = "";
    int invert_moon = 0;
    double latitude = 0;
    double longitude = 0;
    int run_once = 0;
    int show_moon_phase = 0;
    int show_sunrise_sunset = 0;
    int southern_hemisphere = 0;
    char *status_file = NULL;
    double status_file_mt = -1;

    const char *eob = message + sizeof(message);
    char *eol = message;

    while ((option = getopt(argc, argv, "+1b:c:hiMms:z:")) != -1) {
        switch (option) {
          case '1':
            run_once = 1;
            break;

          case 'b':
            battery_data_path_explicit = 1;
            battery_data_path = optarg;
            break;

          case 'c':
            if (!strcmp(optarg, "-")) {
                show_sunrise_sunset = 0;
                break;
            }

            if (strtolatlong(optarg, &latitude, &longitude)) {
                switch (errno) {
                  case EMSGSIZE:
                    fputs("Coordinates too long; lower precision.\n", stderr);
                    break;

                  case EDOM:
                    fputs("Latitude and/or longitude out of range.\n", stderr);
                    break;

                  case EINVAL:
                  default:
                    fputs("Coordinates are malformed.\n", stderr);
                    break;
                }

                return 1;
            }

            show_sunrise_sunset = 1;
            break;

          case 'h':
            usage(basename(argv[0]));
            return 0;

          case 'i':
            invert_moon = 1;
            break;

          case 'm':
            show_moon_phase = 1;
            southern_hemisphere = 0;
            break;

          case 'M':
            show_moon_phase = 1;
            southern_hemisphere = 1;
            break;

          case 's':
            status_file = optarg;
            break;

          case 'z':
            if (altzones_count >= ARRAY_LENGTH(altzones)) {
                fprintf(stderr, "Limit of %lu alternate time zones reached.\n",
                  ARRAY_LENGTH(altzones));
                return 1;
            }

            altzones[altzones_count++] = optarg;
            break;

          case '+':
            // Using "+" to ensure POSIX-style argument parsing is a GNU
            // extension, so an explicit check for "+" as a flag is added for
            // other getopt(3) implementations.
            fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], option);
          default:
            return 1;
        }
    }

    if (optind != argc) {
        fputs("Unexpected command line parameters:", stderr);

        while (optind < argc) {
            fputc(' ', stderr);
            fputs(argv[optind++], stderr);
        }

        fputc('\n', stderr);
        return 1;
    }

    if (!battery_data_path_explicit && access(battery_data_path, R_OK)) {
        battery_data_path = NULL;
    }

    while ((eol = message)) {
        tzset();

        // File I/O is handled after displaying the current time to reduce the
        // chances of disk I/O messing with the clock's monotonicity. The down
        // side is that the file indicators may lag behind by a couple of
        // seconds which could be annoying.
        if (status_file) {
            if ((status_file_mt_now = mtime(status_file)) != status_file_mt) {
                status_file_mt = status_file_mt_now;
                indicators_from_file[0] = '\0';

                if (status_file_mt == -1) {
                    perror(status_file);
                } else {
                    load_indicators_from_file(
                        indicators_from_file, sizeof(indicators_from_file),
                        status_file,
                        SEPARATOR
                    );
                }
            }

            eol = stpcpy(eol, indicators_from_file);
        }

        // Sleep until the turn of the second. The first time the loop is
        // executed, this is step skipped because the clocks haven't been shown
        // yet.
        if (!first) {
            gettimeofday(&tv, NULL);
            tv.tv_sec = 0;
            tv.tv_usec = 1000000 - tv.tv_usec;
            select(0, NULL, NULL, NULL, &tv);
        }

        first = 0;

        if (battery_data_path) {
            eol = stpcpy(eol, battery_indicator(battery_data_path));
            eol = stpcpy(eol, SEPARATOR);
        }

        if (gettimeofday(&tv, NULL) || !(ptm = localtime(&tv.tv_sec))) {
            saved_errno = errno;
            eol = stpcpy(eol, "Unable to get time: ");
            eol = stpcpy(eol, strerror(saved_errno));
            goto display_message;
        }

        now = tv.tv_sec;
        nowtm = *ptm;

        if (show_sunrise_sunset) {
            eol = stpcpy(eol, sunrise_sunset_info(now, latitude, longitude));
            eol = stpcpy(eol, SEPARATOR);
        }

        if (show_moon_phase) {
            eol = stpcpy(eol, moon_icon(now, southern_hemisphere, invert_moon));
            eol = stpcpy(eol, SEPARATOR);
        }

        eol += dow_with_ordinal_dom(eol, (size_t) (eob - eol + 1), &nowtm);

        // Display clocks for any user-defined time zones that differ from the
        // clock for the environment-defined time zone. Since the time zone
        // abbreviation is included in the comparison, time zones with the same
        // UTC offset as the local time but different names will still be shown
        // e.g. "10:10 CKT" (Cook Island Time, UTC-10) and "10:10:37 HST"
        // (Hawaii Standard Time, also UTC-10).
        if (strftime(localclock, sizeof(localclock), "%T %Z", &nowtm)) {
            gmt_to_utc(localclock);
        } else {
            localclock[0] = '\0';
        }

        clocks = eol;

        for (multiple_clocks = 0, k = 0; k < altzones_count; k++) {
            if (altzones_count == 1 && !strcmp("XXX", altzones[k])) {
                tzstrftime(altclock, sizeof(altclock), "", now, altzones[k]);
                break;
            }

            if (tzstrftime(altclock, sizeof(altclock), "%T %Z", now, altzones[k]) &&
                strcmp(altclock, localclock)) {

                // Strip seconds from supplementary clock.
                delete_range(altclock, 5, 3);

                if (!strstr(clocks, altclock)) {
                    eol = stpcpy(eol, SEPARATOR);
                    eol = stpcpy(eol, altclock);
                    multiple_clocks = 1;
                }
            }
        }

        if (localclock[0] != '\0') {
            eol = stpcpy(eol, multiple_clocks ? SEPARATOR : SOFT_SEPARATOR);
            eol = stpcpy(eol, localclock);
        }

display_message:
        if (puts(message) == EOF || fflush(stdout) == EOF) {
            perror(basename(argv[0]));
            return 1;
        }

        if (run_once) {
            break;
        }
    }

    return 0;
}
