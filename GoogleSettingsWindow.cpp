/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "GoogleSettingsWindow.h"

#include <Box.h>
#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include "Constants.h"
#include "DataStore.h"

static const uint32 kMsgChanged = 'gsch';

static const int32 kIntervals[] = { 0, 15, 30, 60, 240 };
static const char* kIntervalNames[] = { "Only when I ask", "Every 15 minutes",
	"Every 30 minutes", "Every hour", "Every 4 hours" };


static BTextView*
MakeNote(const char* name, const char* text, float minHeight)
{
	BTextView* view = new BTextView(name);
	view->SetText(text);
	view->MakeEditable(false);
	view->MakeSelectable(true);
	view->SetWordWrap(true);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	view->SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
	view->SetExplicitMinSize(BSize(420, minHeight));
	return view;
}


GoogleSettingsWindow::GoogleSettingsWindow(BMessenger binder,
	const BMessage& settings)
	:
	BWindow(BRect(0, 0, 480, 400), "Google sync", B_TITLED_WINDOW,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS | B_CLOSE_ON_ESCAPE),
	fBinder(binder),
	fConnected(false),
	fConnecting(false),
	fBusy(false)
{
	BTextView* help = MakeNote("help",
		"Ringbinder syncs the Calendar tab with Google Calendar and the "
		"Address tab with Google Contacts, both ways.\n\n"
		"It connects with your own Google OAuth client, so your data goes "
		"straight between this computer and Google. To get one (once):\n"
		"1. Open console.cloud.google.com and create a project.\n"
		"2. Enable the \"Google Calendar API\" and the \"People API\".\n"
		"3. Set up the OAuth consent screen (External) and add your Google "
		"address as a test user.\n"
		"4. Under Credentials, create an OAuth client ID of type "
		"\"Desktop app\".\n"
		"5. Paste the client ID and secret below and click Connect.",
		190);

	fClientId = new BTextControl("clientId", "Client ID:",
		GetString(settings, "client_id").String(), NULL);
	fClientSecret = new BTextControl("clientSecret", "Client secret:",
		GetString(settings, "client_secret").String(), NULL);
	fClientSecret->TextView()->HideTyping(true);

	fConnect = new BButton("connect", "Connect" B_UTF8_ELLIPSIS,
		new BMessage(MSG_GOOGLE_CONNECT));
	fDisconnect = new BButton("disconnect", "Disconnect",
		new BMessage(MSG_GOOGLE_DISCONNECT));

	bool calendar = true;
	bool contacts = true;
	settings.FindBool("sync_calendar", &calendar);
	settings.FindBool("sync_contacts", &contacts);
	fCalendar = new BCheckBox("calendar",
		"Calendar tab \xE2\x86\x94 Google Calendar",
		new BMessage(kMsgChanged));
	fCalendar->SetValue(calendar ? B_CONTROL_ON : B_CONTROL_OFF);
	BString calendarId = GetString(settings, "calendar_id");
	fCalendarId = new BTextControl("calendarId", "Calendar ID:",
		calendarId.IsEmpty() ? "primary" : calendarId.String(),
		new BMessage(kMsgChanged));
	fCalendarId->SetToolTip("\"primary\" is your main calendar. For another "
		"calendar, use the Calendar ID from its settings page in Google "
		"Calendar.");
	fContacts = new BCheckBox("contacts",
		"Address tab \xE2\x86\x94 Google Contacts",
		new BMessage(kMsgChanged));
	fContacts->SetValue(contacts ? B_CONTROL_ON : B_CONTROL_OFF);

	int32 interval = 30;
	settings.FindInt32("interval", &interval);
	BPopUpMenu* menu = new BPopUpMenu("interval");
	for (int i = 0; i < 5; i++) {
		BMenuItem* item = new BMenuItem(kIntervalNames[i],
			new BMessage(kMsgChanged));
		menu->AddItem(item);
		if (kIntervals[i] == interval)
			item->SetMarked(true);
	}
	if (menu->FindMarked() == NULL)
		menu->ItemAt(2)->SetMarked(true);
	fInterval = new BMenuField("interval", "Sync automatically:", menu);

	fStatus = MakeNote("status", "", 34);
	fSyncNow = new BButton("syncNow", "Sync now", new BMessage(MSG_SYNC_NOW));
	BButton* close = new BButton("close", "Close",
		new BMessage(B_QUIT_REQUESTED));

	BBox* accountBox = new BBox("account");
	accountBox->SetLabel("Google account");
	BGroupView* account = new BGroupView(B_VERTICAL, B_USE_SMALL_SPACING);
	BLayoutBuilder::Group<>(account)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_SPACING,
			B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.AddGrid(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
			.AddTextControl(fClientId, 0, 0)
			.AddTextControl(fClientSecret, 0, 1)
		.End()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fDisconnect)
			.Add(fConnect)
		.End();
	accountBox->AddChild(account);

	BBox* syncBox = new BBox("what");
	syncBox->SetLabel("What to sync");
	BGroupView* what = new BGroupView(B_VERTICAL, B_USE_SMALL_SPACING);
	BLayoutBuilder::Group<>(what)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_SPACING,
			B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(fCalendar)
		.AddGroup(B_HORIZONTAL)
			.AddStrut(20)
			.Add(fCalendarId)
		.End()
		.Add(fContacts)
		.Add(fInterval);
	syncBox->AddChild(what);

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(help)
		.Add(accountBox)
		.Add(syncBox)
		.Add(fStatus)
		.AddGroup(B_HORIZONTAL)
			.Add(fSyncNow)
			.AddGlue()
			.Add(close)
		.End();

	_UpdateState();
	ResizeToPreferred();
	CenterOnScreen();

	// Ask the binder window for the current status
	fBinder.SendMessage(MSG_GOOGLE_STATUS);
}


void
GoogleSettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgChanged:
			_SendSettings();
			_UpdateState();
			break;

		case MSG_GOOGLE_CONNECT:
		{
			BString id(fClientId->Text());
			BString secret(fClientSecret->Text());
			id.Trim();
			secret.Trim();
			if (id.IsEmpty() || id.FindFirst(".apps.googleusercontent.com")
					< 0) {
				fStatus->SetText("Please paste the client ID first. It ends "
					"in \".apps.googleusercontent.com\".");
				fClientId->MakeFocus(true);
				break;
			}
			fClientId->SetText(id.String());
			fClientSecret->SetText(secret.String());
			_SendSettings();
			fBinder.SendMessage(MSG_GOOGLE_CONNECT);
			break;
		}

		case MSG_GOOGLE_DISCONNECT:
			if (fConnecting)
				fBinder.SendMessage(MSG_GOOGLE_CANCEL);
			else
				fBinder.SendMessage(MSG_GOOGLE_DISCONNECT);
			break;

		case MSG_OPEN_SYNC_SETTINGS:
			Activate();
			break;

		case MSG_SYNC_NOW:
			_SendSettings();
			fBinder.SendMessage(MSG_SYNC_NOW);
			break;

		case MSG_GOOGLE_STATUS:
		{
			// An update pushed by the binder window
			const char* text;
			if (message->FindString("status", &text) == B_OK)
				fStatus->SetText(text);
			message->FindBool("connected", &fConnected);
			message->FindBool("connecting", &fConnecting);
			message->FindBool("busy", &fBusy);
			_UpdateState();
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
GoogleSettingsWindow::QuitRequested()
{
	_SendSettings();
	return true;
}


void
GoogleSettingsWindow::_SendSettings()
{
	BMessage message(MSG_GOOGLE_SETTINGS);
	BString id(fClientId->Text());
	BString secret(fClientSecret->Text());
	BString calendarId(fCalendarId->Text());
	id.Trim();
	secret.Trim();
	calendarId.Trim();
	message.AddString("client_id", id);
	message.AddString("client_secret", secret);
	message.AddString("calendar_id",
		calendarId.IsEmpty() ? BString("primary") : calendarId);
	message.AddBool("sync_calendar", fCalendar->Value() == B_CONTROL_ON);
	message.AddBool("sync_contacts", fContacts->Value() == B_CONTROL_ON);
	int32 index = fInterval->Menu()->IndexOf(fInterval->Menu()->FindMarked());
	message.AddInt32("interval", kIntervals[index >= 0 ? index : 2]);
	fBinder.SendMessage(&message);
}


void
GoogleSettingsWindow::_UpdateState()
{
	fClientId->SetEnabled(!fConnected && !fConnecting);
	fClientSecret->SetEnabled(!fConnected && !fConnecting);
	fConnect->SetEnabled(!fConnected && !fConnecting);
	fConnect->SetLabel(fConnected ? "Connected" : "Connect" B_UTF8_ELLIPSIS);
	fDisconnect->SetEnabled(fConnected || fConnecting);
	fDisconnect->SetLabel(fConnecting ? "Cancel" : "Disconnect");
	fCalendarId->SetEnabled(fCalendar->Value() == B_CONTROL_ON);
	fSyncNow->SetEnabled(fConnected && !fBusy
		&& (fCalendar->Value() == B_CONTROL_ON
			|| fContacts->Value() == B_CONTROL_ON));
}
