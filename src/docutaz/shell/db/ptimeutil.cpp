/*
    wdb - weather and water data storage

    Copyright (C) 2007 met.no

    Contact information:
    Norwegian Meteorological Institute
    Box 43 Blindern
    0313 OSLO
    NORWAY
    E-mail: wdb@met.no

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301, USA
*/
#include "ptimeutil.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace
{
    // Copies a broken-down UTC time into a tm by value (the platform calls
    // return a pointer to a shared static buffer, so we copy immediately).
    std::tm gmtimeCopy(std::time_t t)
    {
        std::tm out{};
#if defined(_WIN32)
        gmtime_s(&out, &t);
#else
        if (const std::tm* p = std::gmtime(&t))
            out = *p;
#endif
        return out;
    }

    std::tm localtimeCopy(std::time_t t)
    {
        std::tm out{};
#if defined(_WIN32)
        localtime_s(&out, &t);
#else
        if (const std::tm* p = std::localtime(&t))
            out = *p;
#endif
        return out;
    }

    // Floor-divides a millisecond count into whole seconds since the epoch plus
    // a millisecond remainder in [0, 999]. Floor (not truncation) keeps the
    // remainder non-negative for dates before 1970, so the broken-down calendar
    // time matches what boost::posix_time produced for the same input.
    void splitMillis(long long ms, std::time_t& secs, int& millis)
    {
        long long s = ms / 1000;
        long long r = ms % 1000;
        if (r < 0) {
            r += 1000;
            --s;
        }
        secs = static_cast<std::time_t>(s);
        millis = static_cast<int>(r);
    }
}

namespace miutil
{
    const long long minDate = -9218988800000; // "1677-11-10T17:46:40.001Z"
    const long long maxDate = 9218988800000;  // "2262-02-20T06:13:19.999Z"

    std::string isotimeString(long long millisecondsSinceEpoch, bool useTseparator, bool isLocalFormat)
    {
        const char sep = useTseparator ? 'T' : ' ';

        std::time_t secs = 0;
        int millis = 0;
        splitMillis(millisecondsSinceEpoch, secs, millis);

        char buf[64] = {0};

        if (!isLocalFormat) {
            const std::tm d = gmtimeCopy(secs);
            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d%c%02d:%02d:%02d.%03dZ",
                          d.tm_year + 1900, d.tm_mon + 1, d.tm_mday, sep,
                          d.tm_hour, d.tm_min, d.tm_sec, millis);
            return buf;
        }

        // Local format: reproduce the original behaviour exactly -- apply the
        // machine's *current* UTC offset (resolved to whole minutes) to the
        // supplied instant and append it as a +HH:MM / -HH:MM suffix.
        const std::time_t rawtime = std::time(nullptr);
        const std::tm utc = gmtimeCopy(rawtime);
        const std::tm local = localtimeCopy(rawtime);

        const int utcD = utc.tm_mday;
        int diffH = local.tm_hour - utc.tm_hour;
        if (local.tm_mday < utcD && diffH > 0)
            diffH -= 24;
        else if (local.tm_mday > utcD && diffH < 0)
            diffH += 24;
        const int diffM = local.tm_min - utc.tm_min;

        const long long offsetSecs =
            static_cast<long long>(diffH) * 3600 + static_cast<long long>(diffM) * 60;
        const std::time_t shifted = static_cast<std::time_t>(secs + offsetSecs);
        const std::tm d = gmtimeCopy(shifted);

        // The suffix uses the truncated hour count of the total offset (matching
        // boost::time_duration::hours()) and the absolute raw minute difference.
        const int offsetHours = static_cast<int>(offsetSecs / 3600);

        char utc_buff[8] = {0};
        std::snprintf(utc_buff, sizeof(utc_buff),
                      (offsetHours >= 0) ? "+%02d:%02d" : "%03d:%02d",
                      offsetHours, std::abs(diffM));

        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d%c%02d:%02d:%02d.%03d",
                      d.tm_year + 1900, d.tm_mon + 1, d.tm_mday, sep,
                      d.tm_hour, d.tm_min, d.tm_sec, millis);
        std::strcat(buf, utc_buff);
        return buf;
    }
}
