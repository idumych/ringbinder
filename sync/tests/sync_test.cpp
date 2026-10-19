// Native tests for the sync engine against an in-memory fake of Google's
// Calendar and People APIs. Build: see run_tests.sh
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <map>
#include <string>
#include <vector>

#include "../GoogleAuth.h"
#include "../Json.h"
#include "../SyncCore.h"

static int sFailures = 0;
#define CHECK(cond) do { if (!(cond)) { sFailures++; \
	printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { if ((a) != (b)) { sFailures++; \
	printf("  FAIL %s:%d  %s == %s  (got '%s' vs '%s')\n", __FILE__, \
	__LINE__, #a, #b, std::string(a).c_str(), std::string(b).c_str()); } \
	} while (0)


// ---------------------------------------------------------------- fake Google

static void MergePatch(Json& target, const Json& patch)
{
	for (size_t i = 0; i < patch.Members().size(); i++) {
		const std::string& key = patch.Members()[i].first;
		const Json& value = patch.Members()[i].second;
		if (value.IsNull())
			target.Remove(key);
		else if (value.IsObject() && target.Get(key).IsObject())
			MergePatch(target.At(key), value);
		else
			target.Set(key, value);
	}
}

class FakeGoogle : public HttpTransport {
public:
	std::map<std::string, Json> events;
	std::map<std::string, Json> people;
	std::vector<std::string> log;
	int counter;
	long long clock;

	FakeGoogle() : counter(0), clock(1760500000) {}

	std::string NextEtag() { char b[32]; snprintf(b, 32, "\"e%d\"", ++counter);
		return b; }
	std::string Stamp() {
		clock += 60; time_t t = clock; struct tm u; gmtime_r(&t, &u);
		char b[32]; strftime(b, 32, "%Y-%m-%dT%H:%M:%S.000Z", &u); return b; }

	void Touch(Json& item, bool person) {
		std::string etag = NextEtag();
		item.Set("etag", etag);
		if (person) {
			Json source = Json::Object();
			source.Set("type", "CONTACT");
			source.Set("etag", etag);
			source.Set("updateTime", Stamp());
			Json sources = Json::Array(); sources.Push(source);
			item.At("metadata").Set("sources", sources);
		} else
			item.Set("updated", Stamp());
	}

	Json& AddEvent(const std::string& id, const std::string& json) {
		Json e; Json::Parse(json, e); e.Set("id", id); Touch(e, false);
		return events[id] = e;
	}
	Json& AddPerson(const std::string& id, const std::string& json) {
		Json p; Json::Parse(json, p); p.Set("resourceName", id);
		Touch(p, true); return people[id] = p;
	}

	static std::string Path(const std::string& url) {
		size_t q = url.find('?');
		return q == std::string::npos ? url : url.substr(0, q);
	}
	static HttpResponse Reply(long status, const Json& body) {
		HttpResponse r; r.status = status; r.body = body.Dump(); return r; }
	static HttpResponse Error(long status, const char* message) {
		Json e = Json::Object(); e.At("error").Set("message", message);
		return Reply(status, e); }

	virtual HttpResponse Send(const std::string& method,
		const std::string& url, const std::vector<std::string>& headers,
		const std::string& body)
	{
		log.push_back(method + " " + Path(url));
		bool authorized = false;
		for (size_t i = 0; i < headers.size(); i++)
			if (headers[i] == "Authorization: Bearer TOKEN") authorized = true;
		if (!authorized) return Error(401, "no token");

		Json in;
		if (!body.empty() && !Json::Parse(body, in))
			return Error(400, "bad json");
		std::string path = Path(url);
		const std::string cal
			= "https://www.googleapis.com/calendar/v3/calendars/primary/events";
		const std::string ppl = "https://people.googleapis.com/v1/";

		if (path == cal && method == "GET") {
			Json page = Json::Object(); Json items = Json::Array();
			for (std::map<std::string, Json>::iterator it = events.begin();
					it != events.end(); it++) items.Push(it->second);
			page.Set("items", items); return Reply(200, page);
		}
		if (path == cal && method == "POST") {
			char id[16]; snprintf(id, 16, "ev%d", ++counter);
			in.Set("id", id); in.Set("status", "confirmed");
			Touch(in, false); events[id] = in; return Reply(200, in);
		}
		if (path.compare(0, cal.size() + 1, cal + "/") == 0) {
			std::string id = path.substr(cal.size() + 1);
			if (!events.count(id)) return Error(404, "Not Found");
			if (method == "DELETE") { events.erase(id); HttpResponse r;
				r.status = 204; return r; }
			if (method == "PATCH") { MergePatch(events[id], in);
				Touch(events[id], false); return Reply(200, events[id]); }
		}
		if (path == ppl + "people/me/connections" && method == "GET") {
			Json page = Json::Object(); Json items = Json::Array();
			for (std::map<std::string, Json>::iterator it = people.begin();
					it != people.end(); it++) items.Push(it->second);
			page.Set("connections", items); return Reply(200, page);
		}
		if (path == ppl + "people:createContact" && method == "POST") {
			char id[24]; snprintf(id, 24, "people/c%d", ++counter);
			in.Set("resourceName", id); Touch(in, true);
			people[id] = in; return Reply(200, in);
		}
		if (path.compare(0, ppl.size(), ppl) == 0) {
			std::string rest = path.substr(ppl.size());
			size_t colon = rest.find(':');
			std::string id = rest.substr(0, colon);
			std::string verb = colon == std::string::npos ? "" : rest.substr(colon);
			if (!people.count(id)) return Error(404, "Not Found");
			if (verb == ":deleteContact" && method == "DELETE") {
				people.erase(id); return Reply(200, Json::Object()); }
			if (verb == ":updateContact" && method == "PATCH") {
				// Real API: etag must match and only masked fields change
				if (in.StrAt("etag") != people[id].StrAt("etag"))
					return Error(400, "failedPrecondition: etag mismatch");
				std::string mask = url.substr(url.find("updatePersonFields=") + 19);
				mask = mask.substr(0, mask.find('&'));
				for (size_t p; (p = mask.find("%2C")) != std::string::npos;)
					mask.replace(p, 3, ",");
				mask += ",";
				size_t start = 0, comma;
				while ((comma = mask.find(',', start)) != std::string::npos) {
					std::string field = mask.substr(start, comma - start);
					start = comma + 1;
					if (field.empty()) continue;
					if (field == "names" && in.Get("names").Size() > 1)
						return Error(400, "names is a singleton");
					people[id].Set(field, in.Get(field));
				}
				Touch(people[id], true); return Reply(200, people[id]);
			}
		}
		return Error(404, ("unhandled " + method + " " + path).c_str());
	}
};


// ---------------------------------------------------------------- local store

struct LocalRec {
	long long id; int rev; bool dirty; long long modified;
	std::string gid, etag; FieldMap fields; std::string extra;
};

struct LocalStore {
	std::vector<LocalRec> recs[2];
	std::vector<std::string> tombs[2];
	long long nextId; long long clock;
	LocalStore() : nextId(1), clock(1760500000) {}

	LocalRec* Find(int s, long long id) {
		for (size_t i = 0; i < recs[s].size(); i++)
			if (recs[s][i].id == id) return &recs[s][i];
		return NULL;
	}
	long long Add(int s, const FieldMap& f, const std::string& extra = "") {
		LocalRec r; r.id = nextId++; r.rev = 1; r.dirty = true;
		r.modified = ++clock + 100000; r.fields = f; r.extra = extra;
		recs[s].push_back(r); return r.id;
	}
	void Edit(int s, long long id, const char* key, const std::string& value,
		long long when = 0) {
		LocalRec* r = Find(s, id); r->fields[key] = value; r->rev++;
		r->dirty = true; r->modified = when ? when : (clock += 100000); }
	void Remove(int s, long long id) {
		for (size_t i = 0; i < recs[s].size(); i++) if (recs[s][i].id == id) {
			if (!recs[s][i].gid.empty()) tombs[s].push_back(recs[s][i].gid);
			recs[s].erase(recs[s].begin() + i); return; } }
	LocalRec* ByField(int s, const char* key, const std::string& value) {
		for (size_t i = 0; i < recs[s].size(); i++)
			if (recs[s][i].fields[key] == value) return &recs[s][i];
		return NULL; }

	SyncInput Input() {
		SyncInput in; in.accessToken = "TOKEN"; in.now = 1760500000;
		for (int s = 0; s < 2; s++) for (size_t i = 0; i < recs[s].size(); i++) {
			const LocalRec& r = recs[s][i]; SyncRecord sr;
			sr.localId = r.id; sr.rev = r.rev; sr.dirty = r.dirty;
			sr.modified = r.modified; sr.googleId = r.gid; sr.etag = r.etag;
			sr.fields = r.fields;
			(s == 0 ? in.appointments : in.contacts).push_back(sr); }
		in.calendarTombstones = tombs[0]; in.contactTombstones = tombs[1];
		return in;
	}
	// Mirrors DataStore::ApplySyncResult
	void Apply(const SyncResult& res) {
		for (size_t i = 0; i < res.upserts.size(); i++) {
			const SyncUpsert& u = res.upserts[i];
			if (u.record.localId < 0) {
				LocalRec r; r.id = nextId++; r.rev = 0; r.dirty = false;
				r.modified = 0; r.gid = u.record.googleId;
				r.etag = u.record.etag; r.fields = u.record.fields;
				recs[u.section].push_back(r); continue; }
			LocalRec* r = Find(u.section, u.record.localId);
			if (!r) { if (!u.record.googleId.empty())
				tombs[u.section].push_back(u.record.googleId); continue; }
			r->gid = u.record.googleId; r->etag = u.record.etag;
			if (r->rev != u.baseRev) continue;	// edited meanwhile
			r->fields = u.record.fields; r->dirty = false;
		}
		for (size_t i = 0; i < res.deletes.size(); i++) {
			const SyncDelete& d = res.deletes[i];
			LocalRec* r = Find(d.section, d.localId);
			if (r && r->rev == d.baseRev)
				for (size_t k = 0; k < recs[d.section].size(); k++)
					if (recs[d.section][k].id == d.localId) {
						recs[d.section].erase(recs[d.section].begin() + k); break; }
		}
		const std::vector<std::string>* cleared[2]
			= { &res.clearedCalendarTombstones, &res.clearedContactTombstones };
		for (int s = 0; s < 2; s++) for (size_t i = 0; i < cleared[s]->size(); i++)
			for (size_t k = 0; k < tombs[s].size(); k++)
				if (tombs[s][k] == (*cleared[s])[i]) { tombs[s].erase(tombs[s].begin() + k); break; }
	}
};

static SyncResult Sync(LocalStore& store, FakeGoogle& google) {
	SyncResult res; google.log.clear();
	RunSync(store.Input(), google, res); store.Apply(res);
	for (size_t i = 0; i < res.errors.size(); i++)
		printf("  sync error: %s\n", res.errors[i].c_str());
	return res;
}

static void PrintStats(const SyncResult& r) {
	printf("  stats: push new=%d chg=%d del=%d | pull new=%d chg=%d del=%d\n",
		r.stats.pushedNew, r.stats.pushedChanges, r.stats.pushedDeletes,
		r.stats.pulledNew, r.stats.pulledChanges, r.stats.pulledDeletes);
}

static FieldMap F(const char* a, ...) { // key, value pairs, NULL-terminated
	FieldMap m; va_list ap; va_start(ap, a); const char* k = a;
	while (k) { const char* v = va_arg(ap, const char*); m[k] = v;
		k = va_arg(ap, const char*); } va_end(ap); return m; }

// ---------------------------------------------------------------- tests

static void TestJsonAndCrypto() {
	printf("JSON / SHA-256 / base64url / query parsing\n");
	Json j; std::string err;
	CHECK(Json::Parse("{\"a\":[1,2.5,\"x\\n\\u00e9\\ud83d\\ude00\"],\"b\":null,"
		"\"c\":{\"d\":true}}", j, &err));
	CHECK_EQ(j.Get("a")[2].Str(), "x\n\xC3\xA9\xF0\x9F\x98\x80");
	Json k; CHECK(Json::Parse(j.Dump(), k)); CHECK(j == k);
	CHECK(!Json::Parse("{\"a\":}", k, &err));
	CHECK(!Json::Parse("[1,2", k, &err));
	// FIPS 180-2 test vectors
	CHECK_EQ(GoogleAuth::Base64Url(GoogleAuth::Sha256("abc")),
		"ungWv48Bz-pBQUDeXa4iI7ADYaOWF3qctBD_YfIAFa0");
	CHECK_EQ(GoogleAuth::Base64Url(GoogleAuth::Sha256(
		"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
		"JI1qYdIGOLjlwCaTDD5gOaM85Flk_yFn9uzt1BnbBsE");
	{ const int lens[] = {0,55,56,63,64,65,119,120,1000};
	  const char* want[] = {"47DEQpj8HBSa-_TImW-5JCeuQeRkm5NMpJWZG3hSuFU","1eKFaDzU78AtAhpcYgFGlJWJAQBdb3HongmJ-sd-QHI","BMJiYTcO51QVSdFt7jIMcj4_0UZx5moJmv4KN3wWiI4","dSILRyGCeOZW8gE7uPDEVaJerwHobGSSTp1I2Jd21vI","fOEAlx9k5wAej-WlGXPs3-HO1Cvv5-6NX9YhlQa1OTw","lTfF_fEgSC99WNJentWD9SwCtOME6oFNsWM61WWu1-k","AAtI1O3w-nvuPGI27NJ4W6pdtO64u1Q0GwKeDZ-l-ww","E_BaC1lHh_Xs0xXtyWFBvTJDID0bfU8INvNzCLJ2upg","RPg1RJSlugO6F5Ko0-nFNMR6kYGYD956P0SwbvKufH8"};
	  for (int i = 0; i < 9; i++) CHECK_EQ(GoogleAuth::Base64Url(GoogleAuth::Sha256(std::string(lens[i], 'x'))), want[i]); }
	// RFC 7636 appendix B
	CHECK_EQ(GoogleAuth::Base64Url(GoogleAuth::Sha256(
		"dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk")),
		"E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");
	CHECK_EQ(GoogleAuth::QueryParam("state=a%2Bb&code=4%2F0Ab+c", "code"), "4/0Ab c");
	std::string token = GoogleAuth::RandomToken(64);
	CHECK(token.size() == 64);
	CHECK(SyncDetail::ParseRfc3339("2026-10-15T09:00:00-06:00")
		== SyncDetail::ParseRfc3339("2026-10-15T15:00:00Z"));
	CHECK(SyncDetail::ParseRfc3339("2026-10-15T15:00:00.123Z")
		== SyncDetail::ParseRfc3339("2026-10-15T15:00:00Z"));
	CHECK(SyncDetail::ParseRfc3339("garbage") == -1);
	CHECK_EQ(SyncDetail::LocalToRfc3339Utc("2026-10-15", "09:00", 0),
		"2026-10-15T15:00:00Z");	// Denver is UTC-6 in October
	CHECK_EQ(SyncDetail::LocalToRfc3339Utc("2026-12-15", "09:00", 30),
		"2026-12-15T16:30:00Z");	// UTC-7 in December
}

static void TestCalendar() {
	printf("Calendar two-way sync\n");
	FakeGoogle g; LocalStore s;
	g.AddEvent("remote1", "{\"summary\":\"Dentist\",\"location\":\"Main St\","
		"\"start\":{\"dateTime\":\"2026-10-16T10:00:00-06:00\",\"timeZone\":\"America/Denver\"},"
		"\"end\":{\"dateTime\":\"2026-10-16T10:30:00-06:00\"},"
		"\"attendees\":[{\"email\":\"a@example.com\"}],\"reminders\":{\"useDefault\":false}}");
	g.AddEvent("allday", "{\"summary\":\"Holiday\",\"start\":{\"date\":\"2026-10-19\"},"
		"\"end\":{\"date\":\"2026-10-20\"}}");
	g.AddEvent("bday", "{\"summary\":\"Bob's birthday\",\"eventType\":\"birthday\","
		"\"start\":{\"date\":\"2026-10-20\"},\"end\":{\"date\":\"2026-10-21\"}}");
	g.AddEvent("dup", "{\"summary\":\"Team stand-up\",\"start\":{\"dateTime\":"
		"\"2026-10-12T15:00:00Z\"},\"end\":{\"dateTime\":\"2026-10-12T15:30:00Z\"}}");
	long long lunch = s.Add(0, F("title", "Lunch with Priya", "date", "2026-10-12",
		"start", "13:00", "end", "14:00", "location", "", "notes", "", NULL), "category=Personal");
	long long standup = s.Add(0, F("title", "Team stand-up", "date", "2026-10-12",
		"start", "09:00", "end", "", "location", "", "notes", "", NULL));
	long long party = s.Add(0, F("title", "Halloween party", "date", "2026-10-31",
		"start", "", "end", "", "location", "", "notes", "", NULL));

	SyncResult r = Sync(s, g); PrintStats(r);
	CHECK(r.errors.empty());
	CHECK_EQ(std::to_string(g.events.size()), "6");	// 4 remote + 2 pushed (stand-up linked)
	CHECK_EQ(std::to_string(s.recs[0].size()), "5");	// 3 local + Dentist + Holiday (no birthday)
	LocalRec* dentist = s.ByField(0, "title", "Dentist");
	CHECK(dentist != NULL);
	if (dentist) { CHECK_EQ(dentist->fields["date"], "2026-10-16");
		CHECK_EQ(dentist->fields["start"], "10:00");
		CHECK_EQ(dentist->fields["end"], "10:30"); }
	LocalRec* holiday = s.ByField(0, "title", "Holiday");
	CHECK(holiday && holiday->fields["date"] == "2026-10-19"
		&& holiday->fields["start"].empty());
	CHECK_EQ(s.Find(0, standup)->gid, "dup");		// linked, not duplicated
	CHECK_EQ(s.Find(0, standup)->fields["end"], "09:30");	// filled from Google
	Json& pushed = g.events[s.Find(0, lunch)->gid];
	CHECK_EQ(pushed.Get("start").StrAt("dateTime"), "2026-10-12T19:00:00Z");
	CHECK_EQ(pushed.Get("end").StrAt("dateTime"), "2026-10-12T20:00:00Z");
	Json& allday = g.events[s.Find(0, party)->gid];
	CHECK_EQ(allday.Get("start").StrAt("date"), "2026-10-31");
	CHECK_EQ(allday.Get("end").StrAt("date"), "2026-11-01");

	printf(" second sync must be a no-op\n");
	r = Sync(s, g); PrintStats(r);
	CHECK(r.stats.Total() == 0);
	CHECK_EQ(std::to_string(g.log.size()), "2");	// just the two list calls

	printf(" local edit pushes only the changed field\n");
	s.Edit(0, dentist->id, "title", "Dentist (check-up)");
	r = Sync(s, g); PrintStats(r);
	CHECK(r.stats.pushedChanges == 1);
	CHECK_EQ(g.events["remote1"].StrAt("summary"), "Dentist (check-up)");
	CHECK(g.events["remote1"].Get("attendees").Size() == 1);	// preserved
	CHECK_EQ(g.events["remote1"].Get("start").StrAt("timeZone"), "America/Denver");
	CHECK(Sync(s, g).stats.Total() == 0);

	printf(" moving a timed event to all-day\n");
	s.Edit(0, dentist->id, "start", ""); s.Edit(0, dentist->id, "end", "");
	Sync(s, g);
	CHECK_EQ(g.events["remote1"].Get("start").StrAt("date"), "2026-10-16");
	CHECK(!g.events["remote1"].Get("start").Has("dateTime"));
	CHECK(Sync(s, g).stats.Total() == 0);

	printf(" remote edit is pulled\n");
	g.events["remote1"].Set("location", "Elm Street Clinic"); g.Touch(g.events["remote1"], false);
	r = Sync(s, g); CHECK(r.stats.pulledChanges == 1);
	CHECK_EQ(s.Find(0, dentist->id)->fields["location"], "Elm Street Clinic");

	printf(" conflict: newer edit wins\n");
	g.events["remote1"].Set("summary", "Remote title"); g.Touch(g.events["remote1"], false);
	s.Edit(0, dentist->id, "title", "Older local title", 1000);	// older than remote
	Sync(s, g);
	CHECK_EQ(s.Find(0, dentist->id)->fields["title"], "Remote title");
	g.events["remote1"].Set("summary", "Remote again"); g.Touch(g.events["remote1"], false);
	s.Edit(0, dentist->id, "title", "Newer local title", 4000000000LL);
	Sync(s, g);
	CHECK_EQ(g.events["remote1"].StrAt("summary"), "Newer local title");

	printf(" deletions both ways\n");
	std::string lunchGid = s.Find(0, lunch)->gid;
	s.Remove(0, lunch);
	r = Sync(s, g); CHECK(r.stats.pushedDeletes == 1);
	CHECK(g.events.count(lunchGid) == 0); CHECK(s.tombs[0].empty());
	g.events.erase("allday");
	r = Sync(s, g); CHECK(r.stats.pulledDeletes == 1);
	CHECK(s.ByField(0, "title", "Holiday") == NULL);

	printf(" remote delete + local edit: local edit is kept and re-created\n");
	std::string partyGid = s.Find(0, party)->gid;
	g.events.erase(partyGid);
	s.Edit(0, party, "notes", "Bring pumpkins");
	r = Sync(s, g); CHECK(r.stats.pushedNew == 1);
	CHECK(s.Find(0, party) != NULL && s.Find(0, party)->gid != partyGid);

	printf(" record edited while a sync is running keeps the edit\n");
	SyncInput in = s.Input();
	g.events["remote1"].Set("summary", "Changed on phone"); g.Touch(g.events["remote1"], false);
	SyncResult mid; RunSync(in, g, mid);
	s.Edit(0, dentist->id, "notes", "typed during sync");	// rev changes
	s.Apply(mid);
	CHECK_EQ(s.Find(0, dentist->id)->fields["notes"], "typed during sync");
	CHECK(s.Find(0, dentist->id)->dirty);
	CHECK(Sync(s, g).errors.empty());
	CHECK_EQ(g.events["remote1"].StrAt("description"), "typed during sync");
	CHECK(Sync(s, g).stats.Total() == 0);
	(void)standup;
}

static void TestContacts() {
	printf("Contacts two-way sync\n");
	FakeGoogle g; LocalStore s;
	g.AddPerson("people/c1", "{\"names\":[{\"givenName\":\"Priya\",\"familyName\":\"Chen\","
		"\"displayName\":\"Priya Chen\"}],\"emailAddresses\":[{\"value\":\"priya@lighthouse.example\"}],"
		"\"phoneNumbers\":[{\"value\":\"(303) 555-0176\",\"type\":\"work\"},"
		"{\"value\":\"(720) 555-0101\",\"type\":\"mobile\"},{\"value\":\"(303) 555-9999\",\"type\":\"home\"}],"
		"\"urls\":[{\"value\":\"https://lighthouse.example\"}]}");
	g.AddPerson("people/c2", "{\"names\":[{\"givenName\":\"Lars\",\"familyName\":\"Svensson\"}],"
		"\"organizations\":[{\"name\":\"Mountain Bank\",\"title\":\"Manager\"}],"
		"\"birthdays\":[{\"date\":{\"month\":3,\"day\":9}},{\"date\":{\"year\":1980,\"month\":3,\"day\":9}}],"
		"\"addresses\":[{\"formattedValue\":\"1 Bank Plaza\\nDenver, CO\",\"type\":\"work\"}]}");
	long long priya = s.Add(1, F("last", "Chen", "first", "Priya", "company", "Lighthouse Design",
		"email", "priya@lighthouse.example", NULL));
	long long anna = s.Add(1, F("last", "Becker", "first", "Anna", "phone", "(303) 555-0102",
		"notes", "Met at the conference", NULL));

	SyncResult r = Sync(s, g); PrintStats(r);
	CHECK(r.errors.empty());
	CHECK_EQ(std::to_string(g.people.size()), "3");
	CHECK_EQ(std::to_string(s.recs[1].size()), "3");
	LocalRec* p = s.Find(1, priya);
	CHECK_EQ(p->gid, "people/c1");
	CHECK_EQ(p->fields["mobile"], "(720) 555-0101");	// merged from Google
	CHECK_EQ(g.people["people/c1"].Get("organizations")[0].StrAt("name"), "Lighthouse Design");
	LocalRec* lars = s.ByField(1, "last", "Svensson");
	CHECK(lars && lars->fields["birthday"] == "1980-03-09"
		&& lars->fields["company"] == "Mountain Bank"
		&& lars->fields["address"] == "1 Bank Plaza\nDenver, CO");
	Json& annaRemote = g.people[s.Find(1, anna)->gid];
	CHECK_EQ(annaRemote.Get("biographies")[0].StrAt("value"), "Met at the conference");
	CHECK_EQ(annaRemote.Get("phoneNumbers")[0].StrAt("type"), "work");

	printf(" second sync must be a no-op\n");
	r = Sync(s, g); PrintStats(r); CHECK(r.stats.Total() == 0);

	printf(" editing the work phone keeps the home phone and URL\n");
	s.Edit(1, priya, "phone", "(303) 555-0177");
	r = Sync(s, g); CHECK(r.stats.pushedChanges == 1); CHECK(r.errors.empty());
	Json& c1 = g.people["people/c1"];
	CHECK_EQ(std::to_string(c1.Get("phoneNumbers").Size()), "3");
	CHECK_EQ(c1.Get("phoneNumbers")[0].StrAt("value"), "(303) 555-0177");
	CHECK_EQ(c1.Get("phoneNumbers")[2].StrAt("value"), "(303) 555-9999");
	CHECK(c1.Get("urls").Size() == 1);
	CHECK(Sync(s, g).stats.Total() == 0);

	printf(" renaming, then a remote birthday change\n");
	s.Edit(1, priya, "last", "Chen-Ward");
	CHECK(Sync(s, g).errors.empty());
	CHECK_EQ(c1.Get("names")[0].StrAt("familyName"), "Chen-Ward");
	CHECK(!c1.Get("names")[0].Has("displayName"));
	Json bd = Json::Array(); Json one = Json::Object(); Json date = Json::Object();
	date.Set("year", 1990); date.Set("month", 7); date.Set("day", 4);
	one.Set("date", date); bd.Push(one); c1.Set("birthdays", bd); g.Touch(c1, true);
	Sync(s, g); CHECK_EQ(s.Find(1, priya)->fields["birthday"], "1990-07-04");

	printf(" deletions both ways\n");
	std::string annaGid = s.Find(1, anna)->gid;
	s.Remove(1, anna); r = Sync(s, g);
	CHECK(r.stats.pushedDeletes == 1 && g.people.count(annaGid) == 0);
	g.people.erase("people/c2"); r = Sync(s, g);
	CHECK(r.stats.pulledDeletes == 1 && s.ByField(1, "last", "Svensson") == NULL);
	CHECK(Sync(s, g).stats.Total() == 0);
}

static void TestErrors() {
	printf("Error reporting\n");
	struct Broken : public HttpTransport {
		HttpResponse Send(const std::string&, const std::string& url,
			const std::vector<std::string>&, const std::string&) {
			HttpResponse r;
			if (url.find("calendar") != std::string::npos) {
				r.status = 403;
				r.body = "{\"error\":{\"code\":403,\"message\":\"Google Calendar API "
					"has not been used in project 123 before or it is disabled.\"}}";
			} else r.error = "Could not resolve host";
			return r;
		}
	} broken;
	LocalStore s; s.Add(0, F("title", "x", "date", "2026-10-15", NULL));
	SyncResult r; RunSync(s.Input(), broken, r);
	CHECK(r.errors.size() == 2);
	if (r.errors.size() == 2) {
		printf("  %s\n  %s\n", r.errors[0].c_str(), r.errors[1].c_str());
		CHECK(r.errors[0].find("has not been used") != std::string::npos);
		CHECK(r.errors[1].find("Network error") != std::string::npos);
	}
	CHECK(r.upserts.empty() && r.deletes.empty());
}

int main() {
	setenv("TZ", "America/Denver", 1); tzset();
	TestJsonAndCrypto();
	TestCalendar();
	TestContacts();
	TestErrors();
	printf(sFailures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", sFailures);
	return sFailures ? 1 : 0;
}
