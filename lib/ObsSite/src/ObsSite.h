#pragma once
#include "math.h"
#include <string>
#include <ctime>
#include <memory>
#include <stdexcept>
using namespace std;

//#define OBSSITE_DEBUG                               // Uncomment to turn on debug printing
#ifndef PI
#define PI 3.1415926
#endif
#define degToRadian(deg)   ((deg) * (PI / 180.0))
#define radianToDeg(rad)   ((rad) * (180.0 / PI)) 
#define timegm _mkgmtime

class ObsSite {
public:
    /**
     * @brief Construct a new Obs Site object
     * 
     * @param obsLatDeg     Observation site latitude in degrees
     * @param obsLonDeg     Observation site longitude in degrees
     * @param obsElevM      Observation site elevation in meters MSL
     */
	ObsSite(double obsLatDeg, double obsLonDeg, double obsElevM);

    /**
     * @brief   Get the time of sunrise for the specified year and yday (local time). 
     *          Both year and yday use the same units as the ctime tm structure. 
     *          I.e., year is the number of years since 1900 and yday is the number 
     *          of days since Jan 1 of that year.
     * 
     * @param year      
     * @param yDay
     * @return time_t
     */
	time_t getSunrise(int year, int yday);

	/**
     * @brief   Get the time of solar noon for the specified year and yday (local time). 
     *          Both year and yday use the same units as the ctime tm structure. 
     *          I.e., year is the number of years since 1900 and yday is the number 
     *          of days since Jan 1 of that year.
     * 
     * @param year      
     * @param yDay
     * @return time_t
     */
    time_t getSolarNoon(int year, int yday);

    /**
     * @brief   Get the time of sunset for the specified year and yday (local time). 
     *          Both year and yday use the same units as the ctime tm structure. 
     *          I.e., year is the number of years since 1900 and yday is the number 
     *          of days since Jan 1 of that year.
     * 
     * @param year      
     * @param yDay
     * @return time_t
     */
	time_t getSunset(int year, int yday);

private:
    double latDeg;              // Observing site latitude in degrees
    double lonDeg;              // Observing site longitude in degrees
    double elevM;               // Observing site elevation in meters
    int32_t julianDay;          // The number of days since Jan 1, 2000, 12:00:00 UTC of day used in calculations
    time_t sunriseTime;         // The time of sunrise at the site on julianDay
    time_t transitTime;         // The time of solar noon at the site on julianDay
    time_t sunsetTime;          // The time of sunset at the site on julianDay

    /**
     * @brief   Utility function to calculate sunriseTime, transitTime, and sunsetTime for the date
     *          corresponding to the specified tm-style year and yday
     * 
     * @param year 
     * @param yday 
     */
    void calc(int year, int yday);
};

// Convenience functions for converting between representations

/**
 * @brief Convert a specified julian date to the time_t equivalent
 * 
 * @param jDate     The juliaan date to be converted
 * @return time_t 
 */
time_t julianDateToTime(double jDay);

/**
 * @brief Convert a specified time_t to the julian date equivalent
 * 
 * @param time      The time_t to be converted
 * @return double 
 */
double timeToJulianDate(time_t time);

/**
 * @brief Convert tm-style year and yDay to time_t
 * 
 * @param year 
 * @param yday 
 * @return double 
 */
double yearAndYearDayToTime(int year, int yday);

/***
 *  Utility function to create a string in an fprint-like manner
 */
template<typename ... Args> string string_format(const string& format, Args ... args);

/**
 * @brief Convert a time_t time to a full local time date + time string
 * 
 * @param time      The time to be converted to a string
 * @return string 
 */
string timeToString(time_t time);

/**
 * @brief Convert a time_t time to a local time date string (ignoring the time)
 * 
 * @param time      The time to be converted to a string
 * @return string 
 */
string timeToDateString(time_t time);

/**
 * @brief Convert a time_t time to a local time time string (ignoring the date)
 * 
 * @param time      The time to be converted to a string
 * @return string 
 */
string timeToTimeString(time_t time);

/**
 * @brief Convery a specified julian date to the string equivalent
 * 
 * @param jDate      The julian day to be converted
 * @return string 
 */
string julianDateToString(double jDate);
