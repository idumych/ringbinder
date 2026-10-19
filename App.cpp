/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include <Alert.h>
#include <Application.h>
#include <String.h>

#include <string.h>

#include "BinderWindow.h"
#include "Constants.h"
#include "DataStore.h"


class App : public BApplication {
public:
	App()
		:
		BApplication(APP_SIGNATURE)
	{
	}

	virtual void ReadyToRun()
	{
		status_t status = fStore.Load();
		if (status != B_OK) {
			BString text;
			text.SetToFormat("Your organizer data could not be read (%s).\n\n"
				"Ringbinder will start with an empty binder.",
				strerror(status));
			BAlert* alert = new BAlert(APP_NAME, text.String(), "OK", NULL,
				NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
			alert->Go();
		}
		(new BinderWindow(&fStore))->Show();
	}

private:
	DataStore	fStore;
};


int
main()
{
	App app;
	app.Run();
	return 0;
}
