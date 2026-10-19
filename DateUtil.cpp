/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "DateUtil.h"

#include <stdio.h>
#include <time.h>

const char* kMonthNames[12] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December"
};

const char* kShortMonthNames[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
	"Nov", "Dec"
};

const char* kDayNames[7] = {
	"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
	"Sunday"
};

const char* kShortDayNames[7] = {
	"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};


bool
IsLeapYear(int year)
{
	return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}


int
DaysInMonth(int year, int month)
{
	static const int kDays[12]
		= { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (month < 1 || month > 12)
		return 0;
	if (month == 2 && IsLeapYear(year))
		return 29;
	return kDays[month - 1];
}


bool
IsValidDate(int year, int month, int day)
{
	return year >= 1 && year <= 9999 && month >= 1 && month <= 12
		&& day >= 1 && day <= DaysInMonth(year, month);
}


// Algorithms from Howard Hinnant, "chrono-Compatible Low-Level Date
// Algorithms" (public domain).
long
DayNumber(const SimpleDate& date)
{
	int y = date.year - (date.month <= 2 ? 1 : 0);
	long era = (y >= 0 ? y : y - 399) / 400;
	long yoe = y - era * 400;
	int mp = date.month > 2 ? date.month - 3 : date.month + 9;
	long doy = (153 * mp + 2) / 5 + date.day - 1;
	long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}


SimpleDate
FromDayNumber(long z)
{
	z += 719468;
	long era = (z >= 0 ? z : z - 146096) / 146097;
	long doe = z - era * 146097;
	long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	long y = yoe + era * 400;
	long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	long mp = (5 * doy + 2) / 153;
	int d = doy - (153 * mp + 2) / 5 + 1;
	int m = mp < 10 ? mp + 3 : mp - 9;
	return SimpleDate(y + (m <= 2 ? 1 : 0), m, d);
}


SimpleDate
AddDays(const SimpleDate& date, int days)
{
	return FromDayNumber(DayNumber(date) + days);
}


int
DayOfWeek(const SimpleDate& date)
{
	// 1970-01-01 was a Thursday (index 3 when Monday is 0).
	long n = (DayNumber(date) + 3) % 7;
	if (n < 0)
		n += 7;
	return (int)n;
}


SimpleDate
Today()
{
	time_t now = time(NULL);
	struct tm local;
	localtime_r(&now, &local);
	return SimpleDate(local.tm_year + 1900, local.tm_mon + 1,
		local.tm_mday);
}


SimpleDate
StartOfWeek(const SimpleDate& date)
{
	return AddDays(date, -DayOfWeek(date));
}


int
DaysUntilNext(const SimpleDate& occasion)
{
	SimpleDate today = Today();
	for (int year = today.year; year <= today.year + 8; year++) {
		int day = occasion.day;
		if (occasion.month == 2 && day == 29 && !IsLeapYear(year))
			day = 28;
		SimpleDate next(year, occasion.month, day);
		long diff = DayNumber(next) - DayNumber(today);
		if (diff >= 0)
			return (int)diff;
	}
	return 0;
}


bool
ParseDate(const char* text, SimpleDate& out)
{
	if (text == NULL)
		return false;

	BString string(text);
	string.Trim();
	if (string.ICompare("today") == 0) {
		out = Today();
		return true;
	}
	if (string.ICompare("tomorrow") == 0) {
		out = AddDays(Today(), 1);
		return true;
	}

	int year, month, day;
	char extra;
	if (sscanf(string.String(), "%d-%d-%d%c", &year, &month, &day, &extra)
			!= 3
		&& sscanf(string.String(), "%d/%d/%d%c", &year, &month, &day,
			&extra) != 3) {
		return false;
	}
	if (!IsValidDate(year, month, day))
		return false;

	out = SimpleDate(year, month, day);
	return true;
}


bool
ParseTime(const char* text, int& hour, int& minute)
{
	if (text == NULL)
		return false;

	BString string(text);
	string.Trim();
	char extra;
	int h, m = 0;
	if (sscanf(string.String(), "%d:%d%c", &h, &m, &extra) != 2
		&& sscanf(string.String(), "%d.%d%c", &h, &m, &extra) != 2) {
		if (sscanf(string.String(), "%d%c", &h, &extra) != 1)
			return false;
		m = 0;
	}
	if (h < 0 || h > 23 || m < 0 || m > 59)
		return false;

	hour = h;
	minute = m;
	return true;
}


BString
FormatDate(const SimpleDate& date)
{
	BString result;
	result.SetToFormat("%04d-%02d-%02d", date.year, date.month, date.day);
	return result;
}


BString
FormatShort(const SimpleDate& date)
{
	BString result;
	result.SetToFormat("%d %s %d", date.day,
		kShortMonthNames[date.month - 1], date.year);
	return result;
}


BString
FormatLong(const SimpleDate& date)
{
	BString result;
	result.SetToFormat("%s, %d %s %d", kDayNames[DayOfWeek(date)], date.day,
		kMonthNames[date.month - 1], date.year);
	return result;
}


BString
FormatMonthDay(const SimpleDate& date)
{
	BString result;
	result.SetToFormat("%s %d", kMonthNames[date.month - 1], date.day);
	return result;
}


BString
FormatTime(int hour, int minute)
{
	BString result;
	result.SetToFormat("%02d:%02d", hour, minute);
	return result;
}
