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

#ifndef __RFC1123DATE_H__
#define __RFC1123DATE_H__

#include <string>

namespace miutil {

    // Inclusive bounds (in milliseconds since the Unix epoch) of the dates that
    // can be rendered as a calendar string. Roughly 1677-11-10 .. 2262-02-20,
    // matching the range BSON dates are validated against before formatting.
    extern const long long minDate;
    extern const long long maxDate;

    /**
     * Formats a count of milliseconds since the Unix epoch
     * (1970-01-01T00:00:00Z) as an ISO-8601 compatible string:
     *
     *  - YYYY-MM-DD hh:mm:ss.fff
     *  - YYYY-MM-DDThh:mm:ss.fff   (when useTseparator is true)
     *
     * When isLocalFormat is false the time is rendered in UTC and a trailing
     * 'Z' is appended. When it is true the time is shifted by the machine's
     * current UTC offset and that offset is appended as +HH:MM / -HH:MM.
     *
     * @param millisecondsSinceEpoch milliseconds since 1970-01-01T00:00:00Z.
     * @param useTseparator separate the date and time parts with 'T'.
     * @param isLocalFormat render in local time with a numeric offset suffix.
     * @return the formatted string.
     */
    std::string isotimeString(long long millisecondsSinceEpoch,
                              bool useTseparator = false,
                              bool isLocalFormat = false);

}
#endif
