#include "ObsSite.h"

// Normal member functions

ObsSite::ObsSite(double obsLatDeg, double obsLonDeg, double obsElevM) {
    latDeg = obsLatDeg;
    lonDeg = obsLonDeg;
    elevM = obsElevM;
    #ifdef OBSSITE_DEBUG
    printf("Latitude     %f deg\n", latDeg);
    printf("Longitude    %f deg\n", lonDeg);
    printf("Elevation    %f m\n", elevM);
    #endif
}

time_t ObsSite::getSunrise(int year, int yday) {
    calc(year, yday);
    return sunriseTime;
}

time_t ObsSite::getSolarNoon(int year, int yday) {
    calc(year, yday);
    return transitTime;
}

time_t ObsSite::getSunset(int year, int yday) {
    calc(year, yday);
    return sunsetTime;
}

// Private function ecapsulating the calculation of sunrise, solar noon and sunset
void ObsSite::calc(int year, int yday) {
    // Calculate the julian day from year and yday
    time_t theTime = yearAndYearDayToTime(year, yday);
    double julianDate = timeToJulianDate(theTime);
    int32_t jDay = static_cast <int32_t> (ceil(julianDate - (2451545.0 + 0.0009) + 69.184 / 86400.0));

    // return if we've already done the calculation
    if (jDay == julianDay) {
        return;
    }

    julianDay = jDay;

    #ifdef OBSSITE_DEBUG
    printf("year         %d\n", year);
    printf("yday         %d\n", yday);
    printf("theTime      %d sec\n", theTime);
    printf("julianDate   %f days\n", julianDate);
    printf("julianDay    %d days\n", jDay);
    #endif

    // Calculate jStar, the approximate soalr time from julian day and longitude
    double jStar = julianDay + 0.0009 - lonDeg / 360.0;
    #ifdef OBSSITE_DEBUG
    printf("jStar        %f days\n", jStar);
    #endif

    // Calculate the solar mean anomaly
    double mDeg = fmod(357.5291 + 0.98560028 * jStar, 360);
    double mRadian = degToRadian(mDeg);
    #ifdef OBSSITE_DEBUG
    printf("mDeg         %f degrees\n", mDeg);
    #endif

    // Equation of the center
    double cDeg = 1.9148 * sin(mRadian) + 0.02 * sin(2 * mRadian) + 0.0003 * sin(3 * mRadian);
    #ifdef OBSSITE_DEBUG
    printf("cDeg         %f degrees\n", cDeg);
    #endif

    // Calculate ecliptic longitude
    double lambdaDeg = fmod(mDeg + cDeg + 180.0 + 102.9372, 360);
    #ifdef OBSSITE_DEBUG
    printf("lambdaDeg    %f degrees\n", lambdaDeg);
    #endif
    double lambdaRadian = degToRadian(lambdaDeg);

    // Calculate julian date of solar transit
    double jTransit = 2451545.0 + jStar + 0.0053 * sin(mRadian) - 0.0069 * sin(2 * lambdaRadian);
    transitTime = julianDateToTime(jTransit);
    #ifdef OBSSITE_DEBUG
    printf("transitTime  %s\n", timeToString(transitTime).c_str());
    #endif

    // Calculate the declination of the sun
    double sinDelta = sin(lambdaRadian) * sin(degToRadian(23.4397));
    double deltaRadian = asin(sinDelta);
    double cosDelta = cos(deltaRadian);
    #ifdef OBSSITE_DEBUG
    printf("delta        %f degrees\n", radianToDeg(deltaRadian));
    #endif


    // Calculate the sun's hour angle
    double cosW0 = (sin(degToRadian(-0.833 - 2.076 * sqrt(elevM) / 60.0)) - sin(degToRadian(latDeg)) * sinDelta) / 
    //             (sin(    radians(-0.833 - 2.076 * sqrt(elevM) / 60.0)) - sin(    radians(latDeg)) * sin_d) / 
        (cos(degToRadian(latDeg)) * cosDelta);
    //  (cos(    radians(latDeg)) * cos_d)
    double w0Radian = acos(cosW0);
    double w0Deg = radianToDeg(w0Radian);
    #ifdef OBSSITE_DEBUG
    printf("w0Deg        %f degrees\n", w0Deg);
    #endif

    // Calculate the time of sunrise and sunset

    sunriseTime = julianDateToTime(jTransit - w0Deg / 360.0);
    sunsetTime = julianDateToTime(jTransit + w0Deg / 360.0);
    #ifdef OBSSITE_DEBUG
    printf("sunriseTime  %s\n", timeToString(sunriseTime).c_str());
    printf("sunsetTime   %s\n", timeToString(sunsetTime).c_str());
    #endif
}

// Convenience functions for converting between representations

time_t julianDateToTime(double jDate) {
    return static_cast <time_t> ((jDate - 2440587.5) * 86400);
}

double timeToJulianDate(time_t time) {
    return static_cast <double> (time) / 86400.0 + 2440587.5;
}

double yearAndYearDayToTime(int year, int yday) {
    // Note: This function assumes that the implementation uses time_t that is the number 
    // of seconds since 00:00 hours, Jan 1, 1970 UTC (i.e., the unix timestamp)
    struct tm midnight;
    midnight.tm_year = year;
    midnight.tm_mon = 0;
    midnight.tm_mday = 1;
    midnight.tm_yday = 0;
    midnight.tm_hour = 0;
    midnight.tm_min = 0;
    midnight.tm_sec = 0;

    return mktime(&midnight) + 86400 * yday;    // Midnight Jan 1, of year + seconds/day * yday
}

template<typename ... Args> string string_format(const string& format, Args ... args) {
    int size_s = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
    #ifdef OBSSITE_DEBUG
    if( size_s <= 0) {
        throw runtime_error( "[CSunRiseSet::string_format] Error during formatting." );
    }
    #endif
    auto size = static_cast<size_t>(size_s);
    unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args ...);
    return string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

string timeToString(time_t time) {
    struct tm *pt = localtime(&time);
    char buffer[80];
    strftime(buffer, 80, "%c", pt);
    return string_format("%d = %s (local time)", time, buffer);
}

string timeToDateString(time_t time) {
    struct tm *pt = localtime(&time);
    char buffer[80];
    strftime(buffer, 80, "%F", pt);
    return buffer;
}

string timeToTimeString(time_t time) {
    struct tm *pt = localtime(&time);
    char buffer[80];
    strftime(buffer, 80, "%r", pt);
    return buffer;
}

string julianDateToString(double jDate) {
    return timeToString(julianDateToTime(jDate));
}
