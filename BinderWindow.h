/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#ifndef BINDER_WINDOW_H
#define BINDER_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include "Constants.h"

class BCardView;
class BMenuBar;
class BMenuItem;
class BMessageRunner;
class GoogleSync;
class DataStore;
class RingMargin;
class Section;
class TabStrip;


class BinderWindow : public BWindow {
public:
							BinderWindow(DataStore* store);
	virtual					~BinderWindow();

	virtual	bool			QuitRequested();
	virtual	void			MessageReceived(BMessage* message);

			void			SelectSection(int32 index);

private:
			BMenuBar*		_BuildMenuBar();
			void			_ReloadAll();
			void			_CommitAll();

			void			_StartSync(bool manual);
			void			_SyncDone(BMessage* message);
			void			_ApplyGoogleSettings(BMessage* message);
			void			_UpdateSyncTimer();
			void			_PushSyncStatus();
			void			_Notify(const BString& text, bool error);

			DataStore*		fStore;
			BCardView*		fCards;
			TabStrip*		fLeftTabs;
			RingMargin*		fRings;
			TabStrip*		fRightTabs;
			Section*		fSections[SECTION_COUNT];
			int32			fCurrent;

			GoogleSync*		fSync;
			BMessenger		fSyncWindow;
			BMessageRunner*	fSyncRunner;
			BMenuItem*		fSyncNowItem;
};

#endif // BINDER_WINDOW_H
