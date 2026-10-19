/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "SyncCore.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <set>


static const char* kCalendarBase
	= "https://www.googleapis.com/calendar/v3/calendars/";
static const char* kPeopleBase = "https://people.googleapis.com/v1/";
static const char* kPersonFields = "names,emailAddresses,phoneNumbers,"
	"organizations,addresses,birthdays,biographies,metadata";

static const int kWindowPastDays = 180;
static const int kWindowFutureDays = 540;

static const char* kEventKeys[]
	= { "title", "date", "start", "end", "location", "notes", NULL };
static const char* kContactKeys[] = { "last", "first", "company",
	"jobtitle", "phone", "mobile", "email", "address", "birthday", "notes",
	NULL };

typedef std::set<std::string> KeySet;


// #pragma mark - date and time helpers


static long long
DaysFromCivil(int y, int m, int d)
{
	y -= m <= 2 ? 1 : 0;
	long long era = (y >= 0 ? y : y - 399) / 400;
	long long yoe = y - era * 400;
	int mp = m > 2 ? m - 3 : m + 9;
	long long doy = (153 * mp + 2) / 5 + d - 1;
	long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}


static void
CivilFromDays(long long z, int& y, int& m, int& d)
{
	z += 719468;
	long long era = (z >= 0 ? z : z - 146096) / 146097;
	long long doe = z - era * 146097;
	long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	long long mp = (5 * doy + 2) / 153;
	d = (int)(doy - (153 * mp + 2) / 5 + 1);
	m = (int)(mp < 10 ? mp + 3 : mp - 9);
	y = (int)(yoe + era * 400 + (m <= 2 ? 1 : 0));
}


static bool
ParseIsoDate(const std::string& text, int& y, int& m, int& d)
{
	if (text.size() < 10)
		return false;
	if (sscanf(text.c_str(), "%4d-%2d-%2d", &y, &m, &d) != 3)
		return false;
	return y >= 1 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
}


static std::string
FormatIsoDate(int y, int m, int d)
{
	char buffer[16];
	snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", y, m, d);
	return buffer;
}


static std::string
AddDaysIso(const std::string& date, int days)
{
	int y, m, d;
	if (!ParseIsoDate(date, y, m, d))
		return date;
	CivilFromDays(DaysFromCivil(y, m, d) + days, y, m, d);
	return FormatIsoDate(y, m, d);
}


static bool
ParseHourMinute(const std::string& text, int& h, int& m)
{
	return sscanf(text.c_str(), "%d:%d", &h, &m) == 2 && h >= 0 && h < 24
		&& m >= 0 && m < 60;
}


namespace SyncDetail {

long long
ParseRfc3339(const std::string& text)
{
	int y, mo, d, h = 0, mi = 0, s = 0;
	if (!ParseIsoDate(text, y, mo, d))
		return -1;
	if (text.size() < 19 || (text[10] != 'T' && text[10] != 't'))
		return -1;
	if (sscanf(text.c_str() + 11, "%2d:%2d:%2d", &h, &mi, &s) != 3)
		return -1;

	size_t pos = 19;
	if (pos < text.size() && text[pos] == '.') {
		pos++;
		while (pos < text.size() && isdigit((unsigned char)text[pos]))
			pos++;
	}
	long long offset = 0;
	if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
		int oh = 0, om = 0;
		if (sscanf(text.c_str() + pos + 1, "%2d:%2d", &oh, &om) != 2)
			return -1;
		offset = (oh * 3600LL + om * 60LL) * (text[pos] == '-' ? -1 : 1);
	} else if (pos >= text.size() || (text[pos] != 'Z' && text[pos] != 'z'))
		return -1;

	return DaysFromCivil(y, mo, d) * 86400LL + h * 3600LL + mi * 60LL + s
		- offset;
}


std::string
LocalToRfc3339Utc(const std::string& date, const std::string& time,
	int addMinutes)
{
	int y, mo, d, h, mi;
	if (!ParseIsoDate(date, y, mo, d) || !ParseHourMinute(time, h, mi))
		return std::string();
	struct tm local;
	memset(&local, 0, sizeof(local));
	local.tm_year = y - 1900;
	local.tm_mon = mo - 1;
	local.tm_mday = d;
	local.tm_hour = h;
	local.tm_min = mi + addMinutes;
	local.tm_isdst = -1;
	time_t epoch = mktime(&local);

	struct tm utc;
	gmtime_r(&epoch, &utc);
	char buffer[80];
	snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
		utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
		utc.tm_min, utc.tm_sec);
	return buffer;
}

}	// namespace SyncDetail

using namespace SyncDetail;


static void
EpochToLocal(long long epoch, std::string& date, std::string& time)
{
	time_t value = (time_t)epoch;
	struct tm local;
	localtime_r(&value, &local);
	date = FormatIsoDate(local.tm_year + 1900, local.tm_mon + 1,
		local.tm_mday);
	char buffer[8];
	snprintf(buffer, sizeof(buffer), "%02d:%02d", local.tm_hour,
		local.tm_min);
	time = buffer;
}


static std::string
EpochToRfc3339Utc(long long epoch)
{
	time_t value = (time_t)epoch;
	struct tm utc;
	gmtime_r(&value, &utc);
	char buffer[80];
	snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
		utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
		utc.tm_min, utc.tm_sec);
	return buffer;
}


static std::string
Lower(const std::string& text)
{
	std::string out(text);
	for (size_t i = 0; i < out.size(); i++)
		out[i] = tolower((unsigned char)out[i]);
	return out;
}


static std::string
FieldOf(const FieldMap& fields, const char* key)
{
	FieldMap::const_iterator it = fields.find(key);
	return it != fields.end() ? it->second : std::string();
}


static KeySet
Diff(const FieldMap& a, const FieldMap& b, const char** keys)
{
	KeySet changed;
	for (int i = 0; keys[i] != NULL; i++) {
		if (FieldOf(a, keys[i]) != FieldOf(b, keys[i]))
			changed.insert(keys[i]);
	}
	return changed;
}


// #pragma mark - Google API plumbing


class GoogleApi {
public:
	GoogleApi(HttpTransport& http, const std::string& token)
		: fHttp(http), fToken(token) {}

	// Returns true on 2xx. On failure, error holds a readable message.
	bool Call(const std::string& method, const std::string& url,
		const Json* body, Json* reply, std::string& error, long* status = NULL)
	{
		std::vector<std::string> headers;
		headers.push_back("Authorization: Bearer " + fToken);
		headers.push_back("Accept: application/json");
		if (body != NULL)
			headers.push_back("Content-Type: application/json; charset=utf-8");
		std::string payload = body != NULL ? body->Dump() : std::string();

		HttpResponse response;
		for (int attempt = 0; attempt < 3; attempt++) {
			response = fHttp.Send(method, url, headers, payload);
			// Back off briefly on rate limiting / transient server errors
			if (response.status == 429 || response.status == 500
				|| response.status == 503) {
				sleep(1 + attempt * 2);
				continue;
			}
			break;
		}
		if (status != NULL)
			*status = response.status;

		if (response.status == 0) {
			error = "Network error: " + response.error;
			return false;
		}
		Json json;
		bool parsed = !response.body.empty()
			&& Json::Parse(response.body, json);
		if (response.Ok()) {
			if (reply != NULL && parsed)
				*reply = json;
			return true;
		}

		std::string message = parsed
			? json.Get("error").StrAt("message") : std::string();
		char code[16];
		snprintf(code, sizeof(code), "%ld", response.status);
		if (response.status == 401) {
			error = "Google rejected the sign-in (HTTP 401). "
				"Please connect your account again.";
		} else {
			error = std::string("Google returned HTTP ") + code;
			if (!message.empty())
				error += ": " + message;
		}
		return false;
	}

private:
	HttpTransport&	fHttp;
	std::string		fToken;
};


// #pragma mark - services


class Service {
public:
	virtual ~Service() {}
	virtual const char** Keys() const = 0;
	virtual bool List(std::vector<Json>& remote, std::string& error) = 0;
	virtual std::string IdOf(const Json& item) const = 0;
	virtual std::string EtagOf(const Json& item) const
		{ return item.StrAt("etag"); }
	virtual long long UpdatedOf(const Json& item) const = 0;
	virtual FieldMap ToFields(const Json& item) const = 0;
	virtual bool Insert(const FieldMap& fields, Json& created,
		std::string& error) = 0;
	virtual bool Update(const Json& remote, const FieldMap& fields,
		const KeySet& changed, Json& updated, std::string& error) = 0;
	// true if gone afterwards (deleted now, or already missing)
	virtual bool Delete(const std::string& id, std::string& error) = 0;
	virtual bool InScope(const SyncRecord& record) const = 0;
	virtual bool Matches(const SyncRecord& local, const FieldMap& remote)
		const = 0;
	virtual const char* Noun() const = 0;
};


// Calendar --------------------------------------------------------------

namespace SyncDetail {

FieldMap
EventToFields(const Json& event)
{
	FieldMap fields;
	std::string title = event.StrAt("summary");
	fields["title"] = title.empty() ? "(No title)" : title;
	fields["location"] = event.StrAt("location");
	fields["notes"] = event.StrAt("description");

	const Json& start = event.Get("start");
	const Json& end = event.Get("end");
	if (start.Has("dateTime")) {
		long long startEpoch = ParseRfc3339(start.StrAt("dateTime"));
		long long endEpoch = ParseRfc3339(end.StrAt("dateTime"));
		std::string date, time;
		if (startEpoch >= 0) {
			EpochToLocal(startEpoch, date, time);
			fields["date"] = date;
			fields["start"] = time;
		}
		if (endEpoch >= 0) {
			std::string endDate;
			EpochToLocal(endEpoch, endDate, time);
			fields["end"] = time;
		}
	} else {
		fields["date"] = start.StrAt("date");
		fields["start"] = "";
		fields["end"] = "";
	}
	return fields;
}

}	// namespace SyncDetail


class CalendarService : public Service {
public:
	CalendarService(GoogleApi& api, const std::string& calendarId,
		long long now)
		:
		fApi(api)
	{
		fEventsUrl = std::string(kCalendarBase) + UrlEncode(calendarId)
			+ "/events";
		long long today = now / 86400;
		fFirstDay = today - kWindowPastDays;
		fLastDay = today + kWindowFutureDays;
		fTimeMin = EpochToRfc3339Utc(fFirstDay * 86400);
		fTimeMax = EpochToRfc3339Utc((fLastDay + 1) * 86400);
	}

	virtual const char** Keys() const { return kEventKeys; }
	virtual const char* Noun() const { return "calendar"; }

	virtual bool List(std::vector<Json>& remote, std::string& error)
	{
		std::string pageToken;
		do {
			std::string url = fEventsUrl + "?singleEvents=true"
				"&maxResults=2500&timeMin=" + UrlEncode(fTimeMin)
				+ "&timeMax=" + UrlEncode(fTimeMax);
			if (!pageToken.empty())
				url += "&pageToken=" + UrlEncode(pageToken);
			Json page;
			if (!fApi.Call("GET", url, NULL, &page, error))
				return false;
			const Json& items = page.Get("items");
			for (size_t i = 0; i < items.Size(); i++) {
				const Json& event = items[i];
				if (event.StrAt("status") == "cancelled")
					continue;
				// Skip birthdays, out-of-office, focus time, working location
				std::string type = event.StrAt("eventType");
				if (!type.empty() && type != "default")
					continue;
				remote.push_back(event);
			}
			pageToken = page.StrAt("nextPageToken");
		} while (!pageToken.empty());
		return true;
	}

	virtual std::string IdOf(const Json& event) const
		{ return event.StrAt("id"); }

	virtual long long UpdatedOf(const Json& event) const
		{ return ParseRfc3339(event.StrAt("updated")); }

	virtual FieldMap ToFields(const Json& event) const
		{ return EventToFields(event); }

	virtual bool Insert(const FieldMap& fields, Json& created,
		std::string& error)
	{
		Json body = Json::Object();
		KeySet all;
		for (int i = 0; kEventKeys[i] != NULL; i++)
			all.insert(kEventKeys[i]);
		_Apply(body, fields, all, false);
		return fApi.Call("POST", fEventsUrl, &body, &created, error);
	}

	virtual bool Update(const Json& remote, const FieldMap& fields,
		const KeySet& changed, Json& updated, std::string& error)
	{
		Json patch = Json::Object();
		_Apply(patch, fields, changed, true);
		return fApi.Call("PATCH", fEventsUrl + "/" + UrlEncode(IdOf(remote)),
			&patch, &updated, error);
	}

	virtual bool Delete(const std::string& id, std::string& error)
	{
		long status = 0;
		if (fApi.Call("DELETE", fEventsUrl + "/" + UrlEncode(id), NULL, NULL,
				error, &status))
			return true;
		return status == 404 || status == 410;
	}

	virtual bool InScope(const SyncRecord& record) const
	{
		int y, m, d;
		if (!ParseIsoDate(FieldOf(record.fields, "date"), y, m, d))
			return false;
		long long day = DaysFromCivil(y, m, d);
		return day >= fFirstDay && day <= fLastDay;
	}

	virtual bool Matches(const SyncRecord& local, const FieldMap& remote)
		const
	{
		return FieldOf(local.fields, "date") == FieldOf(remote, "date")
			&& FieldOf(local.fields, "start") == FieldOf(remote, "start")
			&& Lower(FieldOf(local.fields, "title"))
				== Lower(FieldOf(remote, "title"));
	}

private:
	void _Apply(Json& body, const FieldMap& fields, const KeySet& changed,
		bool patch)
	{
		if (changed.count("title"))
			body.Set("summary", FieldOf(fields, "title"));
		if (changed.count("location"))
			body.Set("location", FieldOf(fields, "location"));
		if (changed.count("notes"))
			body.Set("description", FieldOf(fields, "notes"));

		if (!changed.count("date") && !changed.count("start")
			&& !changed.count("end"))
			return;

		std::string date = FieldOf(fields, "date");
		std::string start = FieldOf(fields, "start");
		std::string end = FieldOf(fields, "end");
		Json startJson = Json::Object();
		Json endJson = Json::Object();

		int sh, sm, eh, em;
		if (!ParseHourMinute(start, sh, sm)) {
			// All-day event: end date is exclusive
			startJson.Set("date", date);
			endJson.Set("date", AddDaysIso(date, 1));
			if (patch) {
				startJson.Set("dateTime", Json());
				endJson.Set("dateTime", Json());
			}
		} else {
			startJson.Set("dateTime", LocalToRfc3339Utc(date, start, 0));
			std::string endValue;
			if (ParseHourMinute(end, eh, em)) {
				int length = (eh * 60 + em) - (sh * 60 + sm);
				if (length <= 0)
					length += 24 * 60;	// ends after midnight
				endValue = LocalToRfc3339Utc(date, start, length);
			} else
				endValue = LocalToRfc3339Utc(date, start, 60);
			endJson.Set("dateTime", endValue);
			if (patch) {
				startJson.Set("date", Json());
				endJson.Set("date", Json());
			}
		}
		body.Set("start", startJson);
		body.Set("end", endJson);
	}

	GoogleApi&	fApi;
	std::string	fEventsUrl;
	std::string	fTimeMin;
	std::string	fTimeMax;
	long long	fFirstDay;
	long long	fLastDay;
};


// Contacts --------------------------------------------------------------

static int
FindPhone(const Json& phones, bool mobile)
{
	for (size_t i = 0; i < phones.Size(); i++) {
		bool isMobile = Lower(phones[i].StrAt("type")) == "mobile";
		if (isMobile == mobile)
			return (int)i;
	}
	return -1;
}


namespace SyncDetail {

FieldMap
PersonToFields(const Json& person)
{
	FieldMap fields;
	const Json& name = person.Get("names")[0];
	fields["last"] = name.StrAt("familyName");
	fields["first"] = name.StrAt("givenName");
	if (fields["last"].empty() && fields["first"].empty())
		fields["last"] = name.StrAt("displayName");

	const Json& org = person.Get("organizations")[0];
	fields["company"] = org.StrAt("name");
	fields["jobtitle"] = org.StrAt("title");

	const Json& phones = person.Get("phoneNumbers");
	int phone = FindPhone(phones, false);
	int mobile = FindPhone(phones, true);
	fields["phone"] = phone >= 0 ? phones[phone].StrAt("value") : "";
	fields["mobile"] = mobile >= 0 ? phones[mobile].StrAt("value") : "";

	fields["email"] = person.Get("emailAddresses")[0].StrAt("value");

	const Json& address = person.Get("addresses")[0];
	std::string formatted = address.StrAt("formattedValue");
	fields["address"] = formatted.empty()
		? address.StrAt("streetAddress") : formatted;

	fields["birthday"] = "";
	const Json& birthdays = person.Get("birthdays");
	for (size_t i = 0; i < birthdays.Size(); i++) {
		const Json& date = birthdays[i].Get("date");
		int y = (int)date.Get("year").Num(0);
		int m = (int)date.Get("month").Num(0);
		int d = (int)date.Get("day").Num(0);
		if (y > 0 && m > 0 && d > 0) {
			fields["birthday"] = FormatIsoDate(y, m, d);
			break;
		}
	}

	fields["notes"] = person.Get("biographies")[0].StrAt("value");
	return fields;
}

}	// namespace SyncDetail


class ContactsService : public Service {
public:
	ContactsService(GoogleApi& api) : fApi(api) {}

	virtual const char** Keys() const { return kContactKeys; }
	virtual const char* Noun() const { return "contacts"; }

	virtual bool List(std::vector<Json>& remote, std::string& error)
	{
		std::string pageToken;
		do {
			std::string url = std::string(kPeopleBase)
				+ "people/me/connections?pageSize=1000&personFields="
				+ UrlEncode(kPersonFields);
			if (!pageToken.empty())
				url += "&pageToken=" + UrlEncode(pageToken);
			Json page;
			if (!fApi.Call("GET", url, NULL, &page, error))
				return false;
			const Json& people = page.Get("connections");
			for (size_t i = 0; i < people.Size(); i++) {
				if (people[i].Get("metadata").Get("deleted").Bool())
					continue;
				remote.push_back(people[i]);
			}
			pageToken = page.StrAt("nextPageToken");
		} while (!pageToken.empty());
		return true;
	}

	virtual std::string IdOf(const Json& person) const
		{ return person.StrAt("resourceName"); }

	virtual long long UpdatedOf(const Json& person) const
	{
		const Json& sources = person.Get("metadata").Get("sources");
		long long latest = -1;
		for (size_t i = 0; i < sources.Size(); i++) {
			long long when = ParseRfc3339(sources[i].StrAt("updateTime"));
			if (when > latest)
				latest = when;
		}
		return latest;
	}

	virtual FieldMap ToFields(const Json& person) const
		{ return PersonToFields(person); }

	virtual bool Insert(const FieldMap& fields, Json& created,
		std::string& error)
	{
		Json person = Json::Object();
		KeySet changed;
		for (int i = 0; kContactKeys[i] != NULL; i++) {
			if (!FieldOf(fields, kContactKeys[i]).empty())
				changed.insert(kContactKeys[i]);
		}
		KeySet masks;
		_Apply(person, fields, changed, masks);
		std::string url = std::string(kPeopleBase)
			+ "people:createContact?personFields=" + UrlEncode(kPersonFields);
		return fApi.Call("POST", url, &person, &created, error);
	}

	virtual bool Update(const Json& remote, const FieldMap& fields,
		const KeySet& changed, Json& updated, std::string& error)
	{
		// Start from the full remote person (it carries the etag and
		// metadata.sources the API requires) and change only what differs.
		Json person = remote;
		KeySet masks;
		_Apply(person, fields, changed, masks);
		if (masks.empty()) {
			updated = remote;
			return true;
		}
		std::string mask;
		for (KeySet::const_iterator it = masks.begin(); it != masks.end();
				it++) {
			if (!mask.empty())
				mask += ",";
			mask += *it;
		}
		std::string url = std::string(kPeopleBase) + IdOf(remote)
			+ ":updateContact?updatePersonFields=" + UrlEncode(mask)
			+ "&personFields=" + UrlEncode(kPersonFields);
		return fApi.Call("PATCH", url, &person, &updated, error);
	}

	virtual bool Delete(const std::string& id, std::string& error)
	{
		long status = 0;
		if (fApi.Call("DELETE", std::string(kPeopleBase) + id
				+ ":deleteContact", NULL, NULL, error, &status))
			return true;
		return status == 404;
	}

	virtual bool InScope(const SyncRecord&) const { return true; }

	virtual bool Matches(const SyncRecord& local, const FieldMap& remote)
		const
	{
		const FieldMap& l = local.fields;
		std::string localName = Lower(FieldOf(l, "first") + " "
			+ FieldOf(l, "last"));
		std::string remoteName = Lower(FieldOf(remote, "first") + " "
			+ FieldOf(remote, "last"));
		if (localName != remoteName || localName == " ")
			return false;
		bool localHasContact = !FieldOf(l, "email").empty()
			|| !FieldOf(l, "phone").empty() || !FieldOf(l, "mobile").empty();
		bool remoteHasContact = !FieldOf(remote, "email").empty()
			|| !FieldOf(remote, "phone").empty()
			|| !FieldOf(remote, "mobile").empty();
		if (!localHasContact || !remoteHasContact)
			return true;	// same name, nothing contradicts it
		const char* keys[] = { "email", "phone", "mobile" };
		for (int i = 0; i < 3; i++) {
			std::string value = Lower(FieldOf(l, keys[i]));
			if (!value.empty() && value == Lower(FieldOf(remote, keys[i])))
				return true;
		}
		return false;
	}

private:
	static Json& _ArrayAt(Json& person, const char* key)
	{
		Json& array = person.At(key);
		if (!array.IsArray())
			array = Json::Array();
		return array;
	}

	static void _SetPhone(Json& person, bool mobile, const std::string& value)
	{
		Json& phones = _ArrayAt(person, "phoneNumbers");
		int index = FindPhone(phones, mobile);
		if (value.empty()) {
			if (index >= 0)
				phones.Erase(index);
			return;
		}
		if (index < 0) {
			Json phone = Json::Object();
			phone.Set("value", value);
			phone.Set("type", mobile ? "mobile" : "work");
			phones.Push(phone);
		} else {
			Json& phone = phones.Item(index);
			phone.Set("value", value);
			phone.Remove("canonicalForm");
		}
	}

	void _Apply(Json& person, const FieldMap& fields, const KeySet& changed,
		KeySet& masks)
	{
		if (changed.count("last") || changed.count("first")) {
			Json& names = _ArrayAt(person, "names");
			std::string last = FieldOf(fields, "last");
			std::string first = FieldOf(fields, "first");
			if (last.empty() && first.empty()) {
				names = Json::Array();
			} else {
				if (names.Size() == 0)
					names.Push(Json::Object());
				// names is a singleton field: keep exactly one entry
				while (names.Size() > 1)
					names.Erase(names.Size() - 1);
				Json& name = names.Item(0);
				name.Set("familyName", last);
				name.Set("givenName", first);
				name.Remove("unstructuredName");
				name.Remove("displayName");
				name.Remove("displayNameLastFirst");
			}
			masks.insert("names");
		}

		if (changed.count("company") || changed.count("jobtitle")) {
			Json& orgs = _ArrayAt(person, "organizations");
			std::string company = FieldOf(fields, "company");
			std::string title = FieldOf(fields, "jobtitle");
			if (company.empty() && title.empty()) {
				if (orgs.Size() > 0)
					orgs.Erase(0);
			} else {
				if (orgs.Size() == 0)
					orgs.Push(Json::Object());
				orgs.Item(0).Set("name", company);
				orgs.Item(0).Set("title", title);
			}
			masks.insert("organizations");
		}

		if (changed.count("phone") || changed.count("mobile")) {
			if (changed.count("phone"))
				_SetPhone(person, false, FieldOf(fields, "phone"));
			if (changed.count("mobile"))
				_SetPhone(person, true, FieldOf(fields, "mobile"));
			masks.insert("phoneNumbers");
		}

		if (changed.count("email")) {
			Json& emails = _ArrayAt(person, "emailAddresses");
			std::string email = FieldOf(fields, "email");
			if (email.empty()) {
				if (emails.Size() > 0)
					emails.Erase(0);
			} else {
				if (emails.Size() == 0)
					emails.Push(Json::Object());
				emails.Item(0).Set("value", email);
			}
			masks.insert("emailAddresses");
		}

		if (changed.count("address")) {
			Json& addresses = _ArrayAt(person, "addresses");
			std::string text = FieldOf(fields, "address");
			if (text.empty()) {
				if (addresses.Size() > 0)
					addresses.Erase(0);
			} else {
				std::string type = addresses.Size() > 0
					? addresses[0].StrAt("type") : std::string();
				Json address = Json::Object();
				address.Set("type", type.empty() ? "home" : type);
				address.Set("streetAddress", text);
				address.Set("formattedValue", text);
				if (addresses.Size() == 0)
					addresses.Push(address);
				else
					addresses.Item(0) = address;
			}
			masks.insert("addresses");
		}

		if (changed.count("birthday")) {
			Json& birthdays = _ArrayAt(person, "birthdays");
			birthdays = Json::Array();
			int y, m, d;
			if (ParseIsoDate(FieldOf(fields, "birthday"), y, m, d)) {
				Json date = Json::Object();
				date.Set("year", y);
				date.Set("month", m);
				date.Set("day", d);
				Json birthday = Json::Object();
				birthday.Set("date", date);
				birthdays.Push(birthday);
			}
			masks.insert("birthdays");
		}

		if (changed.count("notes")) {
			Json& bios = _ArrayAt(person, "biographies");
			bios = Json::Array();
			std::string notes = FieldOf(fields, "notes");
			if (!notes.empty()) {
				Json bio = Json::Object();
				bio.Set("value", notes);
				bio.Set("contentType", "TEXT_PLAIN");
				bios.Push(bio);
			}
			masks.insert("biographies");
		}
	}

	GoogleApi&	fApi;
};


// #pragma mark - reconciliation


static void
AddUpsert(SyncResult& result, int section, const SyncRecord& base,
	const FieldMap& fields, const std::string& googleId,
	const std::string& etag)
{
	SyncUpsert upsert;
	upsert.section = section;
	upsert.record = base;
	upsert.record.fields = fields;
	upsert.record.googleId = googleId;
	upsert.record.etag = etag;
	upsert.record.dirty = false;
	upsert.baseRev = base.rev;
	result.upserts.push_back(upsert);
}


static void
Reconcile(Service& service, int section,
	const std::vector<SyncRecord>& locals,
	const std::vector<std::string>& tombstones,
	std::vector<std::string>& cleared, SyncResult& result)
{
	std::string error;
	const char** keys = service.Keys();
	std::string prefix = std::string(section == SYNC_CALENDAR
		? "Calendar" : "Contacts") + ": ";

	// 1. Push local deletions
	for (size_t i = 0; i < tombstones.size(); i++) {
		if (service.Delete(tombstones[i], error)) {
			cleared.push_back(tombstones[i]);
			result.stats.pushedDeletes++;
		} else
			result.errors.push_back(prefix + error);
	}

	// 2. Fetch everything from Google
	std::vector<Json> remote;
	if (!service.List(remote, error)) {
		result.errors.push_back(prefix + error);
		return;
	}
	std::map<std::string, size_t> byId;
	for (size_t i = 0; i < remote.size(); i++)
		byId[service.IdOf(remote[i])] = i;

	KeySet claimed;
	for (size_t i = 0; i < locals.size(); i++) {
		if (!locals[i].googleId.empty())
			claimed.insert(locals[i].googleId);
	}
	for (size_t i = 0; i < tombstones.size(); i++)
		claimed.insert(tombstones[i]);

	// 3. Walk the local records
	for (size_t i = 0; i < locals.size(); i++) {
		const SyncRecord& local = locals[i];

		if (local.googleId.empty()) {
			// Never synced: link to an identical-looking Google entry if
			// there is one (first sync), otherwise create it on Google.
			int match = -1;
			for (size_t j = 0; j < remote.size(); j++) {
				std::string id = service.IdOf(remote[j]);
				if (claimed.count(id) == 0
					&& service.Matches(local, service.ToFields(remote[j]))) {
					match = (int)j;
					break;
				}
			}
			if (match >= 0) {
				const Json& item = remote[match];
				std::string id = service.IdOf(item);
				claimed.insert(id);
				FieldMap remoteFields = service.ToFields(item);
				FieldMap merged = remoteFields;
				for (FieldMap::const_iterator it = local.fields.begin();
						it != local.fields.end(); it++) {
					if (FieldOf(merged, it->first.c_str()).empty())
						merged[it->first] = it->second;
				}
				KeySet changed = Diff(remoteFields, merged, keys);
				std::string etag = service.EtagOf(item);
				if (!changed.empty()) {
					Json updated;
					if (!service.Update(item, merged, changed, updated,
							error)) {
						result.errors.push_back(prefix + error);
						continue;
					}
					etag = service.EtagOf(updated);
					result.stats.pushedChanges++;
				}
				AddUpsert(result, section, local, merged, id, etag);
			} else {
				Json created;
				if (!service.Insert(local.fields, created, error)) {
					result.errors.push_back(prefix + error);
					continue;
				}
				claimed.insert(service.IdOf(created));
				AddUpsert(result, section, local, local.fields,
					service.IdOf(created), service.EtagOf(created));
				result.stats.pushedNew++;
			}
			continue;
		}

		std::map<std::string, size_t>::const_iterator found
			= byId.find(local.googleId);
		if (found == byId.end()) {
			// Gone from Google (or outside the calendar window)
			if (!service.InScope(local))
				continue;
			if (local.dirty) {
				Json created;
				if (!service.Insert(local.fields, created, error)) {
					result.errors.push_back(prefix + error);
					continue;
				}
				AddUpsert(result, section, local, local.fields,
					service.IdOf(created), service.EtagOf(created));
				result.stats.pushedNew++;
			} else {
				SyncDelete removal;
				removal.section = section;
				removal.localId = local.localId;
				removal.baseRev = local.rev;
				result.deletes.push_back(removal);
				result.stats.pulledDeletes++;
			}
			continue;
		}

		const Json& item = remote[found->second];
		FieldMap remoteFields = service.ToFields(item);
		bool remoteChanged = service.EtagOf(item) != local.etag;
		bool localWins = local.dirty
			&& (!remoteChanged || local.modified >= service.UpdatedOf(item));

		if (localWins) {
			KeySet changed = Diff(remoteFields, local.fields, keys);
			std::string etag = service.EtagOf(item);
			if (!changed.empty()) {
				Json updated;
				if (!service.Update(item, local.fields, changed, updated,
						error)) {
					result.errors.push_back(prefix + error);
					continue;
				}
				etag = service.EtagOf(updated);
				result.stats.pushedChanges++;
			}
			AddUpsert(result, section, local, local.fields, local.googleId,
				etag);
		} else if (remoteChanged || local.dirty) {
			if (!Diff(remoteFields, local.fields, keys).empty())
				result.stats.pulledChanges++;
			AddUpsert(result, section, local, remoteFields, local.googleId,
				service.EtagOf(item));
		}
	}

	// 4. New on Google
	for (size_t i = 0; i < remote.size(); i++) {
		std::string id = service.IdOf(remote[i]);
		if (claimed.count(id) != 0)
			continue;
		SyncRecord fresh;
		AddUpsert(result, section, fresh, service.ToFields(remote[i]), id,
			service.EtagOf(remote[i]));
		result.stats.pulledNew++;
	}
}


void
RunSync(const SyncInput& input, HttpTransport& http, SyncResult& result)
{
	GoogleApi api(http, input.accessToken);
	long long now = input.now > 0 ? input.now : (long long)time(NULL);

	if (input.syncCalendar) {
		CalendarService calendar(api, input.calendarId.empty()
			? std::string("primary") : input.calendarId, now);
		Reconcile(calendar, SYNC_CALENDAR, input.appointments,
			input.calendarTombstones, result.clearedCalendarTombstones,
			result);
	}
	if (input.syncContacts) {
		ContactsService contacts(api);
		Reconcile(contacts, SYNC_CONTACTS, input.contacts,
			input.contactTombstones, result.clearedContactTombstones, result);
	}
}
