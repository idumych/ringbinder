/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * All organizer data lives in one flattened BMessage in
 * ~/config/settings/Ringbinder/organizer.data. Each section holds a list of
 * records; a record is a BMessage of string/bool fields plus an int64 "id".
 * Records that take part in Google sync also carry "rev" (int32), "modified"
 * (int64), "g_dirty" (bool), "g_id" and "g_etag" (strings).
 */
#ifndef DATA_STORE_H
#define DATA_STORE_H

#include <Message.h>
#include <Rect.h>
#include <String.h>

#include <map>
#include <vector>

typedef std::vector<BMessage> RecordList;


class DataStore {
public:
							DataStore();

			status_t		Load();
			status_t		Save();

			RecordList&		Records(const char* section);
			const BMessage*	Find(const char* section, int64 id);

			// Local edits: bump the revision, mark the record as needing
			// sync, and keep any Google link the stored record already has.
			int64			Put(const char* section, BMessage& record);
			// Local delete: remembers the Google id so the sync deletes it
			// on Google too.
			void			Remove(const char* section, int64 id);

			// Used by the sync to store results without marking them dirty
			int64			PutSynced(const char* section, BMessage& record);
			void			RemoveSynced(const char* section, int64 id);

			std::vector<BString>& Tombstones(const char* section);
			BMessage&		GoogleSettings() { return fGoogle; }

			BRect			WindowFrame() const { return fWindowFrame; }
			void			SetWindowFrame(BRect frame)
								{ fWindowFrame = frame; }
			int32			LastSection() const { return fLastSection; }
			void			SetLastSection(int32 section)
								{ fLastSection = section; }

private:
			void			_SeedFirstRun();

			std::map<BString, RecordList> fData;
			std::map<BString, std::vector<BString> > fTombstones;
			BMessage		fGoogle;
			int64			fNextId;
			BRect			fWindowFrame;
			int32			fLastSection;
};


// Record field helpers
BString	GetString(const BMessage& record, const char* key);
void	SetString(BMessage& record, const char* key, const char* value);
bool	GetBool(const BMessage& record, const char* key);
void	SetBool(BMessage& record, const char* key, bool value);
int64	GetId(const BMessage& record);

#endif // DATA_STORE_H
