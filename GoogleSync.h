/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * Connects the sync engine (sync/SyncCore) to the app: runs sign-in and
 * sync on background threads and applies results to the DataStore on the
 * window thread. Only the window thread touches the DataStore.
 */
#ifndef GOOGLE_SYNC_H
#define GOOGLE_SYNC_H

#include <Messenger.h>
#include <String.h>

class DataStore;


class GoogleSync {
public:
							GoogleSync(DataStore* store, BMessenger target);

			bool			IsConnected();
			bool			IsBusy() const { return fSyncing; }
			bool			IsConnecting() const { return fConnecting; }

			// Sign-in. The result arrives as MSG_GOOGLE_CONNECTED.
			status_t		StartConnect(const BString& clientId,
								const BString& clientSecret);
			void			CancelConnect();
			// Returns an error text, or an empty string on success
			BString			HandleConnected(BMessage* message);
			void			Disconnect();
			// Forget all Google links for one section (e.g. after the
			// calendar ID changed), so nothing is mistaken for a deletion.
			void			ResetLinks(const char* section);

			// Sync. The result arrives as MSG_SYNC_DONE.
			status_t		StartSync(bool manual);
			// Applies the result; returns a one-line summary for the user.
			// *hadErrors / *changes describe the outcome.
			BString			HandleSyncDone(BMessage* message,
								bool* hadErrors, int32* changes,
								bool* manual);

			BString			StatusText();

private:
			DataStore*		fStore;
			BMessenger		fTarget;
			bool			fSyncing;
			bool			fConnecting;
};

#endif // GOOGLE_SYNC_H
