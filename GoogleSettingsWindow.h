/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * "Google sync" settings: OAuth client, what to sync, how often, status.
 * Talks to the binder window only through messages.
 */
#ifndef GOOGLE_SETTINGS_WINDOW_H
#define GOOGLE_SETTINGS_WINDOW_H

#include <Messenger.h>
#include <Window.h>

class BButton;
class BCheckBox;
class BMenuField;
class BStringView;
class BTextControl;
class BTextView;


class GoogleSettingsWindow : public BWindow {
public:
							GoogleSettingsWindow(BMessenger binder,
								const BMessage& settings);

	virtual	void			MessageReceived(BMessage* message);
	virtual	bool			QuitRequested();

private:
			void			_SendSettings();
			void			_UpdateState();

			BMessenger		fBinder;
			BTextControl*	fClientId;
			BTextControl*	fClientSecret;
			BButton*		fConnect;
			BButton*		fDisconnect;
			BCheckBox*		fCalendar;
			BTextControl*	fCalendarId;
			BCheckBox*		fContacts;
			BMenuField*		fInterval;
			BTextView*		fStatus;
			BButton*		fSyncNow;

			bool			fConnected;
			bool			fConnecting;
			bool			fBusy;
};

#endif // GOOGLE_SETTINGS_WINDOW_H
