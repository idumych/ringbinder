/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "DataStore.h"

#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>

#include <time.h>

#include "Constants.h"
#include "DateUtil.h"

static const char* kDataFileName = "organizer.data";
static const char* kTempFileName = "organizer.data.tmp";
static const uint32 kArchiveWhat = 'RBdt';


static status_t
SettingsDirectory(BPath& path)
{
	status_t status = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (status != B_OK)
		return status;
	status = path.Append(APP_NAME);
	if (status != B_OK)
		return status;
	return create_directory(path.Path(), 0755);
}


DataStore::DataStore()
	:
	fNextId(1),
	fWindowFrame(60, 50, 1040, 730),
	fLastSection(SECTION_CALENDAR)
{
}


status_t
DataStore::Load()
{
	BPath path;
	status_t status = SettingsDirectory(path);
	if (status != B_OK)
		return status;
	path.Append(kDataFileName);

	BFile file(path.Path(), B_READ_ONLY);
	status = file.InitCheck();
	if (status == B_ENTRY_NOT_FOUND) {
		_SeedFirstRun();
		return B_OK;
	}
	if (status != B_OK)
		return status;

	BMessage archive;
	status = archive.Unflatten(&file);
	if (status != B_OK) {
		// Keep the unreadable file so the next save can't overwrite it
		file.Unset();
		BEntry(path.Path()).Rename("organizer.data.unreadable", true);
		return status;
	}

	fData.clear();
	for (int32 i = 0; i < SECTION_COUNT; i++) {
		RecordList& list = fData[BString(kSectionKeys[i])];
		for (int32 j = 0;; j++) {
			BMessage record;
			if (archive.FindMessage(kSectionKeys[i], j, &record) != B_OK)
				break;
			int64 id = GetId(record);
			if (id >= fNextId)
				fNextId = id + 1;
			list.push_back(record);
		}
	}

	fTombstones.clear();
	for (int32 i = 0; i < SECTION_COUNT; i++) {
		BString key("tombstones:");
		key << kSectionKeys[i];
		const char* id;
		for (int32 j = 0; archive.FindString(key.String(), j, &id) == B_OK;
				j++)
			fTombstones[BString(kSectionKeys[i])].push_back(BString(id));
	}
	if (archive.FindMessage("google", &fGoogle) != B_OK)
		fGoogle.MakeEmpty();

	BRect frame;
	if (archive.FindRect("window_frame", &frame) == B_OK && frame.IsValid())
		fWindowFrame = frame;
	int32 section;
	if (archive.FindInt32("section", &section) == B_OK
		&& section >= 0 && section < SECTION_COUNT) {
		fLastSection = section;
	}
	return B_OK;
}


status_t
DataStore::Save()
{
	BMessage archive(kArchiveWhat);
	std::map<BString, RecordList>::iterator it;
	for (it = fData.begin(); it != fData.end(); it++) {
		for (size_t i = 0; i < it->second.size(); i++)
			archive.AddMessage(it->first.String(), &it->second[i]);
	}
	std::map<BString, std::vector<BString> >::iterator tomb;
	for (tomb = fTombstones.begin(); tomb != fTombstones.end(); tomb++) {
		BString key("tombstones:");
		key << tomb->first;
		for (size_t i = 0; i < tomb->second.size(); i++)
			archive.AddString(key.String(), tomb->second[i]);
	}
	archive.AddMessage("google", &fGoogle);
	archive.AddRect("window_frame", fWindowFrame);
	archive.AddInt32("section", fLastSection);

	BPath directory;
	status_t status = SettingsDirectory(directory);
	if (status != B_OK)
		return status;

	// Write to a temporary file first, then atomically replace the old one
	BPath tempPath(directory.Path(), kTempFileName);
	BFile file(tempPath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	status = file.InitCheck();
	if (status != B_OK)
		return status;
	status = archive.Flatten(&file);
	if (status != B_OK)
		return status;
	file.Sync();
	file.Unset();

	BEntry entry(tempPath.Path());
	return entry.Rename(kDataFileName, true);
}


RecordList&
DataStore::Records(const char* section)
{
	return fData[BString(section)];
}


const BMessage*
DataStore::Find(const char* section, int64 id)
{
	RecordList& list = Records(section);
	for (size_t i = 0; i < list.size(); i++) {
		if (GetId(list[i]) == id)
			return &list[i];
	}
	return NULL;
}


int64
DataStore::Put(const char* section, BMessage& record)
{
	int32 rev = 0;
	const BMessage* stored = Find(section, GetId(record));
	if (stored != NULL) {
		stored->FindInt32("rev", &rev);
		// An editor window may hold a copy from before the record was
		// linked to Google; never lose the link.
		SetString(record, "g_id", GetString(*stored, "g_id").String());
		SetString(record, "g_etag", GetString(*stored, "g_etag").String());
	}
	record.RemoveName("rev");
	record.AddInt32("rev", rev + 1);
	record.RemoveName("modified");
	record.AddInt64("modified", (int64)time(NULL));
	SetBool(record, "g_dirty", true);
	return PutSynced(section, record);
}


int64
DataStore::PutSynced(const char* section, BMessage& record)
{
	int64 id = GetId(record);
	if (id <= 0) {
		id = fNextId++;
		record.RemoveName("id");
		record.AddInt64("id", id);
	}

	RecordList& list = Records(section);
	for (size_t i = 0; i < list.size(); i++) {
		if (GetId(list[i]) == id) {
			list[i] = record;
			return id;
		}
	}
	list.push_back(record);
	return id;
}


void
DataStore::Remove(const char* section, int64 id)
{
	const BMessage* stored = Find(section, id);
	if (stored != NULL) {
		BString googleId = GetString(*stored, "g_id");
		if (!googleId.IsEmpty())
			Tombstones(section).push_back(googleId);
	}
	RemoveSynced(section, id);
}


void
DataStore::RemoveSynced(const char* section, int64 id)
{
	RecordList& list = Records(section);
	for (size_t i = 0; i < list.size(); i++) {
		if (GetId(list[i]) == id) {
			list.erase(list.begin() + i);
			return;
		}
	}
}


std::vector<BString>&
DataStore::Tombstones(const char* section)
{
	return fTombstones[BString(section)];
}


void
DataStore::_SeedFirstRun()
{
	BMessage note;
	SetString(note, "title", "Welcome to Ringbinder");
	SetString(note, "body",
		"Ringbinder keeps your diary, tasks, contacts, calls, year planner, "
		"notes and anniversaries in one binder.\n\n"
		"\xE2\x80\xA2 Click the coloured tabs on the right (or press "
		"Alt+1 \xE2\x80\xA6 Alt+7) to turn to a section.\n"
		"\xE2\x80\xA2 Double-click an empty day in the Calendar to add an "
		"appointment; double-click an entry to edit it.\n"
		"\xE2\x80\xA2 In the Planner, drag across days to block out a "
		"vacation or trip.\n"
		"\xE2\x80\xA2 Anniversaries and due To Do items show up on the "
		"Calendar automatically.\n\n"
		"Everything is saved as you go, in "
		"~/config/settings/Ringbinder.");
	Put(kSectionKeys[SECTION_NOTEPAD], note);
	fLastSection = SECTION_CALENDAR;
}


// #pragma mark - record helpers


BString
GetString(const BMessage& record, const char* key)
{
	const char* value;
	if (record.FindString(key, &value) == B_OK)
		return BString(value);
	return BString();
}


void
SetString(BMessage& record, const char* key, const char* value)
{
	record.RemoveName(key);
	record.AddString(key, value != NULL ? value : "");
}


bool
GetBool(const BMessage& record, const char* key)
{
	bool value;
	if (record.FindBool(key, &value) == B_OK)
		return value;
	return false;
}


void
SetBool(BMessage& record, const char* key, bool value)
{
	record.RemoveName(key);
	record.AddBool(key, value);
}


int64
GetId(const BMessage& record)
{
	int64 id;
	if (record.FindInt64("id", &id) == B_OK)
		return id;
	return -1;
}
