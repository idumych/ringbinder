/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * Two-way synchronization between Ringbinder records and Google Calendar /
 * Google Contacts (People API). Plain C++11 with no Haiku dependencies: the
 * app converts its BMessage records to SyncRecord on the way in and applies
 * the SyncResult on the way out.
 *
 * Rules
 *  - A record linked to Google carries its Google id and etag.
 *  - Local edits mark a record dirty; dirty records are pushed.
 *  - If both sides changed since the last sync, the newer edit wins.
 *  - Only fields that actually differ are sent, so details Ringbinder does
 *    not show (extra phone numbers, attendees, reminders...) are preserved.
 *  - Deleting a linked record locally deletes it on Google (tombstones);
 *    a record deleted on Google is deleted locally unless it has unsynced
 *    local edits, in which case it is re-created on Google.
 *  - Calendar sync covers a window (180 days back, 540 ahead); records
 *    outside it are left alone.
 */
#ifndef RB_SYNC_CORE_H
#define RB_SYNC_CORE_H

#include <map>
#include <string>
#include <vector>

#include "Http.h"
#include "Json.h"


typedef std::map<std::string, std::string> FieldMap;


struct SyncRecord {
	long long	localId;	// -1 for records created by the sync
	int			rev;		// local revision the sync started from
	bool		dirty;		// edited locally since last sync
	long long	modified;	// epoch seconds of the last local edit
	std::string	googleId;	// event id or contact resourceName
	std::string	etag;
	FieldMap	fields;		// only the fields that are synced

	SyncRecord() : localId(-1), rev(0), dirty(false), modified(0) {}
};


struct SyncInput {
	bool		syncCalendar;
	bool		syncContacts;
	std::string	calendarId;		// "primary" by default
	std::string	accessToken;
	long long	now;			// epoch seconds, for the calendar window

	std::vector<SyncRecord>		appointments;
	std::vector<SyncRecord>		contacts;
	std::vector<std::string>	calendarTombstones;
	std::vector<std::string>	contactTombstones;

	SyncInput() : syncCalendar(true), syncContacts(true),
		calendarId("primary"), now(0) {}
};


enum SyncSection { SYNC_CALENDAR = 0, SYNC_CONTACTS = 1 };


struct SyncUpsert {
	int			section;
	SyncRecord	record;		// localId -1 = new local record
	int			baseRev;	// apply only if the local rev still matches
};

struct SyncDelete {
	int			section;
	long long	localId;
	int			baseRev;
};

struct SyncStats {
	int	pushedNew, pushedChanges, pushedDeletes;
	int	pulledNew, pulledChanges, pulledDeletes;

	SyncStats() : pushedNew(0), pushedChanges(0), pushedDeletes(0),
		pulledNew(0), pulledChanges(0), pulledDeletes(0) {}
	int Total() const { return pushedNew + pushedChanges + pushedDeletes
		+ pulledNew + pulledChanges + pulledDeletes; }
};

struct SyncResult {
	std::vector<SyncUpsert>		upserts;
	std::vector<SyncDelete>		deletes;
	std::vector<std::string>	clearedCalendarTombstones;
	std::vector<std::string>	clearedContactTombstones;
	std::vector<std::string>	errors;
	SyncStats					stats;
};


void RunSync(const SyncInput& input, HttpTransport& http, SyncResult& result);


// Exposed for tests
namespace SyncDetail {
	FieldMap	EventToFields(const Json& event);
	FieldMap	PersonToFields(const Json& person);
	long long	ParseRfc3339(const std::string& text);	// -1 if invalid
	std::string	LocalToRfc3339Utc(const std::string& date,
					const std::string& time, int addMinutes);
}

#endif // RB_SYNC_CORE_H
