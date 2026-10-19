/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "BinderWindow.h"

#include <Alert.h>
#include <Application.h>
#include <CardLayout.h>
#include <CardView.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Notification.h>

#include "DataStore.h"
#include "GoogleSettingsWindow.h"
#include "GoogleSync.h"
#include "Section.h"
#include "TabStrip.h"


BinderWindow::BinderWindow(DataStore* store)
	:
	BWindow(store->WindowFrame(), APP_NAME, B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS
			| B_QUIT_ON_WINDOW_CLOSE),
	fStore(store),
	fCurrent(-1),
	fSync(NULL),
	fSyncRunner(NULL),
	fSyncNowItem(NULL)
{
	fSync = new GoogleSync(store, BMessenger(this));

	fCards = new BCardView("pages");
	for (int32 i = 0; i < SECTION_COUNT; i++) {
		fSections[i] = CreateSection(i, store);
		fCards->AddChild(fSections[i]);
	}
	fTabs = new TabStrip();

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(_BuildMenuBar())
		.AddGroup(B_HORIZONTAL, 0)
			.Add(new BinderSpine())
			.Add(fCards)
			.Add(fTabs)
		.End();

	SelectSection(store->LastSection());
	MoveOnScreen();

	_UpdateSyncTimer();
	// Sync shortly after start-up when automatic sync is on
	int32 interval = 30;
	fStore->GoogleSettings().FindInt32("interval", &interval);
	if (fSync->IsConnected() && interval > 0) {
		BMessageRunner::StartSending(BMessenger(this),
			new BMessage(MSG_SYNC_TIMER), 3000000, 1);
	}
}


BinderWindow::~BinderWindow()
{
	delete fSyncRunner;
	delete fSync;
}


BMenuBar*
BinderWindow::_BuildMenuBar()
{
	BMenuBar* bar = new BMenuBar("menubar");

	BMenu* menu = new BMenu("File");
	menu->AddItem(new BMenuItem("Save", new BMessage(MSG_SAVE_ALL), 'S'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("About " APP_NAME B_UTF8_ELLIPSIS,
		new BMessage(MSG_ABOUT)));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED),
		'Q'));
	bar->AddItem(menu);

	menu = new BMenu("Entry");
	menu->AddItem(new BMenuItem("New" B_UTF8_ELLIPSIS,
		new BMessage(MSG_NEW_ENTRY), 'N'));
	menu->AddItem(new BMenuItem("Edit" B_UTF8_ELLIPSIS,
		new BMessage(MSG_EDIT_ENTRY), 'E'));
	menu->AddItem(new BMenuItem("Delete" B_UTF8_ELLIPSIS,
		new BMessage(MSG_DELETE_ENTRY), 'D'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Mark done / not done",
		new BMessage(MSG_TOGGLE_DONE), 'K'));
	bar->AddItem(menu);

	menu = new BMenu("View");
	menu->AddItem(new BMenuItem("Previous", new BMessage(MSG_PREV),
		B_LEFT_ARROW));
	menu->AddItem(new BMenuItem("Next", new BMessage(MSG_NEXT),
		B_RIGHT_ARROW));
	menu->AddItem(new BMenuItem("Today", new BMessage(MSG_GO_TODAY), 'T'));
	bar->AddItem(menu);

	menu = new BMenu("Sync");
	fSyncNowItem = new BMenuItem("Sync with Google now",
		new BMessage(MSG_SYNC_NOW), 'Y');
	menu->AddItem(fSyncNowItem);
	menu->AddItem(new BMenuItem("Google sync settings" B_UTF8_ELLIPSIS,
		new BMessage(MSG_OPEN_SYNC_SETTINGS)));
	bar->AddItem(menu);

	menu = new BMenu("Section");
	for (int32 i = 0; i < SECTION_COUNT; i++) {
		BMessage* message = new BMessage(MSG_SELECT_SECTION);
		message->AddInt32("index", i);
		menu->AddItem(new BMenuItem(kSectionNames[i], message, '1' + i));
	}
	bar->AddItem(menu);

	return bar;
}


void
BinderWindow::SelectSection(int32 index)
{
	if (index < 0 || index >= SECTION_COUNT || index == fCurrent)
		return;

	if (fCurrent >= 0)
		fSections[fCurrent]->Commit();

	fCurrent = index;
	fCards->CardLayout()->SetVisibleItem(index);
	fTabs->SetSelected(index);

	BString title(APP_NAME " \xE2\x80\x94 ");
	title << kSectionNames[index];
	SetTitle(title.String());
}


void
BinderWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_SELECT_SECTION:
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK)
				SelectSection(index);
			break;
		}

		// Menu commands go to whichever page is showing
		case MSG_NEW_ENTRY:
		case MSG_EDIT_ENTRY:
		case MSG_DELETE_ENTRY:
		case MSG_TOGGLE_DONE:
		case MSG_PREV:
		case MSG_NEXT:
		case MSG_GO_TODAY:
			if (fCurrent >= 0)
				PostMessage(message, fSections[fCurrent]);
			break;

		case MSG_RECORD_SAVED:
		{
			const char* section;
			BMessage record;
			if (message->FindString("section", &section) == B_OK
				&& message->FindMessage("record", &record) == B_OK) {
				fStore->Put(section, record);
				fStore->Save();
				_ReloadAll();
			}
			break;
		}

		case MSG_DATA_CHANGED:
			_ReloadAll();
			break;

		// Google sync
		case MSG_SYNC_NOW:
			_StartSync(true);
			break;
		case MSG_SYNC_TIMER:
			_StartSync(false);
			break;
		case MSG_SYNC_DONE:
			_SyncDone(message);
			break;

		case MSG_OPEN_SYNC_SETTINGS:
		{
			// Bring an open settings window to the front, or make one
			if (fSyncWindow.IsValid()
				&& fSyncWindow.SendMessage(MSG_OPEN_SYNC_SETTINGS) == B_OK)
				break;
			GoogleSettingsWindow* window = new GoogleSettingsWindow(
				BMessenger(this), fStore->GoogleSettings());
			fSyncWindow = BMessenger(window);
			window->Show();
			break;
		}

		case MSG_GOOGLE_SETTINGS:
			_ApplyGoogleSettings(message);
			break;

		case MSG_GOOGLE_STATUS:
			_PushSyncStatus();
			break;

		case MSG_GOOGLE_CONNECT:
		{
			BMessage& settings = fStore->GoogleSettings();
			if (fSync->StartConnect(GetString(settings, "client_id"),
					GetString(settings, "client_secret")) != B_OK) {
				_Notify("Could not start Google sign-in.", true);
			}
			_PushSyncStatus();
			break;
		}

		case MSG_GOOGLE_CANCEL:
			fSync->CancelConnect();
			break;

		case MSG_GOOGLE_CONNECTED:
		{
			BString error = fSync->HandleConnected(message);
			if (!error.IsEmpty()) {
				SetString(fStore->GoogleSettings(), "last_status",
					error.String());
				_Notify(error, true);
			} else {
				_UpdateSyncTimer();
				_StartSync(true);
			}
			_PushSyncStatus();
			break;
		}

		case MSG_GOOGLE_DISCONNECT:
		{
			BAlert* alert = new BAlert("Disconnect",
				"Disconnect Ringbinder from your Google account?\n\n"
				"Your appointments and contacts stay in Ringbinder and in "
				"Google; they just stop syncing.",
				"Cancel", "Disconnect", NULL, B_WIDTH_AS_USUAL,
				B_WARNING_ALERT);
			alert->SetShortcut(0, B_ESCAPE);
			if (alert->Go() == 1) {
				fSync->Disconnect();
				_UpdateSyncTimer();
			}
			_PushSyncStatus();
			break;
		}

		case MSG_SAVE_ALL:
			_CommitAll();
			fStore->SetWindowFrame(Frame());
			fStore->SetLastSection(fCurrent);
			if (fStore->Save() != B_OK) {
				BAlert* alert = new BAlert(APP_NAME,
					"Your organizer could not be saved.", "OK", NULL, NULL,
					B_WIDTH_AS_USUAL, B_STOP_ALERT);
				alert->Go(NULL);
			}
			break;

		case MSG_ABOUT:
		{
			BAlert* alert = new BAlert("About " APP_NAME,
				APP_NAME " 1.0\n\n"
				"A tabbed personal organizer for Haiku, in the spirit of "
				"the classic desktop organizers of the late 1990s.\n\n"
				"Calendar \xE2\x80\xA2 To Do \xE2\x80\xA2 Address \xE2\x80\xA2 "
				"Calls \xE2\x80\xA2 Planner \xE2\x80\xA2 Notepad \xE2\x80\xA2 "
				"Anniversary",
				"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_INFO_ALERT);
			alert->Go(NULL);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
BinderWindow::QuitRequested()
{
	_CommitAll();
	fStore->SetWindowFrame(Frame());
	fStore->SetLastSection(fCurrent);
	fStore->Save();
	return true;
}


void
BinderWindow::_StartSync(bool manual)
{
	if (!fSync->IsConnected()) {
		if (manual)
			PostMessage(MSG_OPEN_SYNC_SETTINGS);
		return;
	}
	if (fSync->IsBusy())
		return;

	_CommitAll();
	if (fSync->StartSync(manual) == B_OK && fSyncNowItem != NULL) {
		fSyncNowItem->SetLabel("Syncing" B_UTF8_ELLIPSIS);
		fSyncNowItem->SetEnabled(false);
	}
	_PushSyncStatus();
}


void
BinderWindow::_SyncDone(BMessage* message)
{
	bool hadErrors = false;
	bool manual = false;
	int32 changes = 0;
	BString summary = fSync->HandleSyncDone(message, &hadErrors, &changes,
		&manual);

	if (fSyncNowItem != NULL) {
		fSyncNowItem->SetLabel("Sync with Google now");
		fSyncNowItem->SetEnabled(true);
	}
	if (changes > 0)
		_ReloadAll();

	// Quiet when automatic syncs find nothing to do
	if (hadErrors || changes > 0 || manual)
		_Notify(summary, hadErrors);
	_PushSyncStatus();
}


void
BinderWindow::_ApplyGoogleSettings(BMessage* message)
{
	BMessage& settings = fStore->GoogleSettings();

	const char* text;
	if (message->FindString("client_id", &text) == B_OK)
		SetString(settings, "client_id", text);
	if (message->FindString("client_secret", &text) == B_OK)
		SetString(settings, "client_secret", text);

	if (message->FindString("calendar_id", &text) == B_OK) {
		BString old = GetString(settings, "calendar_id");
		if (old.IsEmpty())
			old = "primary";
		if (old != text) {
			// Links point into the old calendar; drop them so its events
			// are not taken for deletions.
			fSync->ResetLinks(kSectionKeys[SECTION_CALENDAR]);
		}
		SetString(settings, "calendar_id", text);
	}

	bool value;
	if (message->FindBool("sync_calendar", &value) == B_OK)
		SetBool(settings, "sync_calendar", value);
	if (message->FindBool("sync_contacts", &value) == B_OK)
		SetBool(settings, "sync_contacts", value);
	int32 interval;
	if (message->FindInt32("interval", &interval) == B_OK) {
		settings.RemoveName("interval");
		settings.AddInt32("interval", interval);
	}

	fStore->Save();
	_UpdateSyncTimer();
}


void
BinderWindow::_UpdateSyncTimer()
{
	delete fSyncRunner;
	fSyncRunner = NULL;

	int32 interval = 30;
	fStore->GoogleSettings().FindInt32("interval", &interval);
	if (interval > 0 && fSync->IsConnected()) {
		BMessage timer(MSG_SYNC_TIMER);
		fSyncRunner = new BMessageRunner(BMessenger(this), &timer,
			(bigtime_t)interval * 60 * 1000000);
	}
}


void
BinderWindow::_PushSyncStatus()
{
	if (!fSyncWindow.IsValid())
		return;
	BMessage status(MSG_GOOGLE_STATUS);
	status.AddString("status", fSync->StatusText());
	status.AddBool("connected", fSync->IsConnected());
	status.AddBool("connecting", fSync->IsConnecting());
	status.AddBool("busy", fSync->IsBusy());
	fSyncWindow.SendMessage(&status);
}


void
BinderWindow::_Notify(const BString& text, bool error)
{
	BNotification notification(error
		? B_ERROR_NOTIFICATION : B_INFORMATION_NOTIFICATION);
	notification.SetGroup(APP_NAME);
	notification.SetTitle("Google sync");
	notification.SetContent(text);
	notification.SetMessageID("ringbinder-sync");
	notification.Send();
}


void
BinderWindow::_ReloadAll()
{
	for (int32 i = 0; i < SECTION_COUNT; i++)
		fSections[i]->Reload();
}


void
BinderWindow::_CommitAll()
{
	for (int32 i = 0; i < SECTION_COUNT; i++)
		fSections[i]->Commit();
}
