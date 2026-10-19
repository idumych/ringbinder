/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "GoogleSync.h"

#include <OS.h>
#ifndef APP_COSMOE
#	include <Url.h>
#endif

#include <stdio.h>
#include <time.h>
#ifdef APP_COSMOE
#	include <spawn.h>
#	include <sys/wait.h>
extern char** environ;
#endif

#include "Constants.h"
#include "DataStore.h"
#include "sync/GoogleAuth.h"
#include "sync/Http.h"
#include "sync/SyncCore.h"


static const char* kCalendarKeys[]
	= { "title", "date", "start", "end", "location", "notes", NULL };
static const char* kContactKeys[] = { "last", "first", "company",
	"jobtitle", "phone", "mobile", "email", "address", "birthday", "notes",
	NULL };

// Opens the sign-in page in the user's web browser
static status_t
OpenInBrowser(const std::string& url)
{
#ifdef APP_COSMOE
	// Cosmoe's BUrl can't launch the Linux desktop's browser
	char* argv[] = { const_cast<char*>("xdg-open"),
		const_cast<char*>(url.c_str()), NULL };
	pid_t pid;
	if (posix_spawnp(&pid, "xdg-open", NULL, NULL, argv, environ) != 0)
		return B_ERROR;
	int status;
	if (waitpid(pid, &status, 0) < 0)
		return B_ERROR;
	return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? B_OK : B_ERROR;
#else
	BUrl browserUrl(url.c_str(), false);
	return browserUrl.OpenWithPreferredApplication(false);
#endif
}


// Set by CancelConnect(); polled once a second by the sign-in thread
static volatile int sCancelConnect = 0;


// #pragma mark - background jobs


struct ConnectJob {
	std::string	clientId;
	std::string	clientSecret;
	BMessenger	target;
};


static int32
ConnectThread(void* data)
{
	ConnectJob* job = static_cast<ConnectJob*>(data);
	BMessage reply(MSG_GOOGLE_CONNECTED);

	GoogleAuth::LoopbackFlow flow;
	std::string error;
	std::string code;
	if (!flow.Start(error)) {
		reply.AddString("error", error.c_str());
	} else {
		std::string url = flow.AuthorizationUrl(job->clientId,
			GoogleAuth::kScopes);
		if (OpenInBrowser(url) != B_OK) {
			reply.AddString("error", "Could not open the web browser.");
		} else if (!flow.WaitForCode(5 * 60, &sCancelConnect, code, error)) {
			reply.AddString("error", error.c_str());
		} else {
			CurlTransport http;
			GoogleAuth::Tokens tokens = flow.Exchange(http, job->clientId,
				job->clientSecret, code);
			if (!tokens.error.empty())
				reply.AddString("error", tokens.error.c_str());
			else if (tokens.refreshToken.empty()) {
				reply.AddString("error", "Google did not grant offline "
					"access. Remove Ringbinder under your Google Account's "
					"third-party access page and connect again.");
			} else
				reply.AddString("refresh_token", tokens.refreshToken.c_str());
		}
	}

	job->target.SendMessage(&reply);
	delete job;
	return 0;
}


struct SyncJob {
	std::string	clientId;
	std::string	clientSecret;
	std::string	refreshToken;
	bool		manual;
	SyncInput	input;
	SyncResult	result;
	BMessenger	target;
};


static int32
SyncThread(void* data)
{
	SyncJob* job = static_cast<SyncJob*>(data);

	CurlTransport http;
	GoogleAuth::Tokens tokens = GoogleAuth::Refresh(http, job->clientId,
		job->clientSecret, job->refreshToken);
	if (!tokens.error.empty())
		job->result.errors.push_back(tokens.error);
	else {
		job->input.accessToken = tokens.accessToken;
		RunSync(job->input, http, job->result);
	}

	BMessage reply(MSG_SYNC_DONE);
	reply.AddPointer("job", job);
	if (job->target.SendMessage(&reply) != B_OK)
		delete job;	// window is gone
	return 0;
}


// #pragma mark - record conversion


static SyncRecord
ToSyncRecord(const BMessage& message, const char** keys)
{
	SyncRecord record;
	record.localId = GetId(message);
	int32 rev = 0;
	message.FindInt32("rev", &rev);
	record.rev = rev;
	record.dirty = GetBool(message, "g_dirty");
	int64 modified = 0;
	message.FindInt64("modified", &modified);
	record.modified = modified;
	record.googleId = GetString(message, "g_id").String();
	record.etag = GetString(message, "g_etag").String();
	for (int i = 0; keys[i] != NULL; i++)
		record.fields[keys[i]] = GetString(message, keys[i]).String();
	return record;
}


static void
CopyRecords(DataStore* store, const char* section, const char** keys,
	std::vector<SyncRecord>& out)
{
	RecordList& records = store->Records(section);
	for (size_t i = 0; i < records.size(); i++)
		out.push_back(ToSyncRecord(records[i], keys));
}


static void
CopyTombstones(DataStore* store, const char* section,
	std::vector<std::string>& out)
{
	std::vector<BString>& tombstones = store->Tombstones(section);
	for (size_t i = 0; i < tombstones.size(); i++)
		out.push_back(tombstones[i].String());
}


static void
ClearTombstones(DataStore* store, const char* section,
	const std::vector<std::string>& cleared)
{
	std::vector<BString>& tombstones = store->Tombstones(section);
	for (size_t i = 0; i < cleared.size(); i++) {
		for (size_t j = 0; j < tombstones.size(); j++) {
			if (tombstones[j] == cleared[i].c_str()) {
				tombstones.erase(tombstones.begin() + j);
				break;
			}
		}
	}
}


static const char*
SectionKey(int section)
{
	return section == SYNC_CALENDAR ? kSectionKeys[SECTION_CALENDAR]
		: kSectionKeys[SECTION_ADDRESS];
}


static void
ApplyResult(DataStore* store, const SyncResult& result)
{
	for (size_t i = 0; i < result.upserts.size(); i++) {
		const SyncUpsert& upsert = result.upserts[i];
		const char* section = SectionKey(upsert.section);
		const char** keys = upsert.section == SYNC_CALENDAR
			? kCalendarKeys : kContactKeys;
		const SyncRecord& synced = upsert.record;

		BMessage record;
		if (synced.localId >= 0) {
			const BMessage* stored = store->Find(section, synced.localId);
			if (stored == NULL) {
				// Deleted locally while the sync ran: delete it on Google
				// next time too.
				if (!synced.googleId.empty()) {
					store->Tombstones(section).push_back(
						BString(synced.googleId.c_str()));
				}
				continue;
			}
			record = *stored;
			SetString(record, "g_id", synced.googleId.c_str());
			SetString(record, "g_etag", synced.etag.c_str());

			int32 rev = 0;
			record.FindInt32("rev", &rev);
			if (rev != upsert.baseRev) {
				// Edited while the sync ran: keep the edit (still dirty) and
				// only record the link so it is pushed next time.
				store->PutSynced(section, record);
				continue;
			}
		} else {
			record.AddInt32("rev", 0);
			record.AddInt64("modified", 0);
		}

		for (int k = 0; keys[k] != NULL; k++) {
			FieldMap::const_iterator it = synced.fields.find(keys[k]);
			SetString(record, keys[k],
				it != synced.fields.end() ? it->second.c_str() : "");
		}
		SetString(record, "g_id", synced.googleId.c_str());
		SetString(record, "g_etag", synced.etag.c_str());
		SetBool(record, "g_dirty", false);
		store->PutSynced(section, record);
	}

	for (size_t i = 0; i < result.deletes.size(); i++) {
		const SyncDelete& removal = result.deletes[i];
		const char* section = SectionKey(removal.section);
		const BMessage* stored = store->Find(section, removal.localId);
		if (stored == NULL)
			continue;
		int32 rev = 0;
		stored->FindInt32("rev", &rev);
		if (rev == removal.baseRev)
			store->RemoveSynced(section, removal.localId);
	}

	ClearTombstones(store, kSectionKeys[SECTION_CALENDAR],
		result.clearedCalendarTombstones);
	ClearTombstones(store, kSectionKeys[SECTION_ADDRESS],
		result.clearedContactTombstones);
}


// #pragma mark - GoogleSync


GoogleSync::GoogleSync(DataStore* store, BMessenger target)
	:
	fStore(store),
	fTarget(target),
	fSyncing(false),
	fConnecting(false)
{
}


bool
GoogleSync::IsConnected()
{
	return !GetString(fStore->GoogleSettings(), "refresh_token").IsEmpty();
}


status_t
GoogleSync::StartConnect(const BString& clientId, const BString& clientSecret)
{
	if (fConnecting)
		return B_BUSY;

	sCancelConnect = 0;
	ConnectJob* job = new ConnectJob;
	job->clientId = clientId.String();
	job->clientSecret = clientSecret.String();
	job->target = fTarget;

	thread_id thread = spawn_thread(ConnectThread, "google sign-in",
		B_NORMAL_PRIORITY, job);
	if (thread < 0) {
		delete job;
		return thread;
	}
	fConnecting = true;
	return resume_thread(thread);
}


void
GoogleSync::CancelConnect()
{
	sCancelConnect = 1;
}


BString
GoogleSync::HandleConnected(BMessage* message)
{
	fConnecting = false;
	const char* error;
	if (message->FindString("error", &error) == B_OK)
		return BString(error);

	const char* token;
	if (message->FindString("refresh_token", &token) != B_OK)
		return BString("Sign-in failed.");

	BMessage& settings = fStore->GoogleSettings();
	SetString(settings, "refresh_token", token);
	SetString(settings, "last_status", "Connected. Not synced yet.");
	fStore->Save();
	return BString();
}


void
GoogleSync::Disconnect()
{
	CancelConnect();
	BMessage& settings = fStore->GoogleSettings();
	SetString(settings, "refresh_token", "");
	SetString(settings, "last_status", "");

	// Forget links so a later connection (possibly to another account)
	// starts cleanly instead of deleting or duplicating things. The first
	// sync after reconnecting matches entries up again.
	ResetLinks(kSectionKeys[SECTION_CALENDAR]);
	ResetLinks(kSectionKeys[SECTION_ADDRESS]);
	fStore->Save();
}


void
GoogleSync::ResetLinks(const char* section)
{
	RecordList& records = fStore->Records(section);
	for (size_t i = 0; i < records.size(); i++) {
		SetString(records[i], "g_id", "");
		SetString(records[i], "g_etag", "");
	}
	fStore->Tombstones(section).clear();
}


status_t
GoogleSync::StartSync(bool manual)
{
	if (fSyncing)
		return B_BUSY;
	if (!IsConnected())
		return B_NOT_ALLOWED;

	BMessage& settings = fStore->GoogleSettings();
	SyncJob* job = new SyncJob;
	job->clientId = GetString(settings, "client_id").String();
	job->clientSecret = GetString(settings, "client_secret").String();
	job->refreshToken = GetString(settings, "refresh_token").String();
	job->manual = manual;
	job->target = fTarget;

	bool calendar = true;
	bool contacts = true;
	settings.FindBool("sync_calendar", &calendar);
	settings.FindBool("sync_contacts", &contacts);
	job->input.syncCalendar = calendar;
	job->input.syncContacts = contacts;
	BString calendarId = GetString(settings, "calendar_id");
	job->input.calendarId = calendarId.IsEmpty()
		? "primary" : calendarId.String();
	job->input.now = (long long)time(NULL);

	if (calendar) {
		CopyRecords(fStore, kSectionKeys[SECTION_CALENDAR], kCalendarKeys,
			job->input.appointments);
		CopyTombstones(fStore, kSectionKeys[SECTION_CALENDAR],
			job->input.calendarTombstones);
	}
	if (contacts) {
		CopyRecords(fStore, kSectionKeys[SECTION_ADDRESS], kContactKeys,
			job->input.contacts);
		CopyTombstones(fStore, kSectionKeys[SECTION_ADDRESS],
			job->input.contactTombstones);
	}

	thread_id thread = spawn_thread(SyncThread, "google sync",
		B_LOW_PRIORITY, job);
	if (thread < 0) {
		delete job;
		return thread;
	}
	fSyncing = true;
	return resume_thread(thread);
}


BString
GoogleSync::HandleSyncDone(BMessage* message, bool* hadErrors,
	int32* changes, bool* manual)
{
	fSyncing = false;
	SyncJob* job = NULL;
	if (message->FindPointer("job", (void**)&job) != B_OK || job == NULL)
		return BString();

	ApplyResult(fStore, job->result);

	const SyncStats& stats = job->result.stats;
	int pulled = stats.pulledNew + stats.pulledChanges + stats.pulledDeletes;
	int pushed = stats.pushedNew + stats.pushedChanges + stats.pushedDeletes;

	BString summary;
	if (!job->result.errors.empty()) {
		summary = job->result.errors[0].c_str();
		if (job->result.errors.size() > 1) {
			BString more;
			more.SetToFormat(" (and %d more problems)",
				(int)job->result.errors.size() - 1);
			summary << more;
		}
	} else if (pulled + pushed == 0) {
		summary = "Everything is up to date.";
	} else {
		summary.SetToFormat("%d change%s from Google, %d sent to Google.",
			pulled, pulled == 1 ? "" : "s", pushed);
	}

	time_t now = time(NULL);
	struct tm local;
	localtime_r(&now, &local);
	char when[32];
	strftime(when, sizeof(when), "%d %b, %H:%M", &local);
	BString status;
	status << "Last sync " << when << ": " << summary;

	BMessage& settings = fStore->GoogleSettings();
	SetString(settings, "last_status", status.String());
	fStore->Save();

	*hadErrors = !job->result.errors.empty();
	*changes = pulled + pushed;
	*manual = job->manual;
	delete job;
	return summary;
}


BString
GoogleSync::StatusText()
{
	if (fConnecting)
		return BString("Waiting for you to sign in to Google in the "
			"browser\xE2\x80\xA6");
	if (fSyncing)
		return BString("Syncing\xE2\x80\xA6");
	if (!IsConnected())
		return BString("Not connected.");
	BString status = GetString(fStore->GoogleSettings(), "last_status");
	return status.IsEmpty() ? BString("Connected.") : status;
}
