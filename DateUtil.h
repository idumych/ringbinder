/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * Small self-contained calendar arithmetic (proleptic Gregorian).
 * Dates are stored in records as ISO strings: YYYY-MM-DD.
 */
#ifndef DATE_UTIL_H
#define DATE_UTIL_H

#include <String.h>

struct SimpleDate {
	int year;
	int month;	// 1..12
	int day;	// 1..31

	SimpleDate() : year(1970), month(1), day(1) {}
	SimpleDate(int y, int m, int d) : year(y), month(m), day(d) {}

	bool operator==(const SimpleDate& other) const
		{ return year == other.year && month == other.month
			&& day == other.day; }
	bool operator!=(const SimpleDate& other) const
		{ return !(*this == other); }
};

extern const char* kMonthNames[12];
extern const char* kShortMonthNames[12];
extern const char* kDayNames[7];		// Monday first
extern const char* kShortDayNames[7];

bool		IsLeapYear(int year);
int			DaysInMonth(int year, int month);
bool		IsValidDate(int year, int month, int day);

long		DayNumber(const SimpleDate& date);	// days since 1970-01-01
SimpleDate	FromDayNumber(long days);
SimpleDate	AddDays(const SimpleDate& date, int days);
int			DayOfWeek(const SimpleDate& date);	// 0 = Monday .. 6 = Sunday
SimpleDate	Today();
SimpleDate	StartOfWeek(const SimpleDate& date);
int			DaysUntilNext(const SimpleDate& occasion);

bool		ParseDate(const char* text, SimpleDate& out);
bool		ParseTime(const char* text, int& hour, int& minute);

BString		FormatDate(const SimpleDate& date);		// 2026-10-15
BString		FormatShort(const SimpleDate& date);	// 15 Oct 2026
BString		FormatLong(const SimpleDate& date);		// Thursday, 15 October 2026
BString		FormatMonthDay(const SimpleDate& date);	// October 15
BString		FormatTime(int hour, int minute);		// 09:30

#endif // DATE_UTIL_H
