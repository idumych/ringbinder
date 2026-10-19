/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * A generic record editor built from a table of field descriptions. When the
 * user saves, it sends MSG_RECORD_SAVED ("section", "record") to its target.
 */
#ifndef EDIT_WINDOW_H
#define EDIT_WINDOW_H

#include <Messenger.h>
#include <String.h>
#include <Window.h>

#include <vector>

enum FieldType {
	FIELD_TEXT,
	FIELD_MULTILINE,
	FIELD_DATE,
	FIELD_TIME,
	FIELD_CHECK,
	FIELD_CHOICE
};

struct FieldSpec {
	const char*	key;
	const char*	label;
	FieldType	type;
	const char*	choices;	// "A|B|C" for FIELD_CHOICE
	bool		required;
};


class EditWindow : public BWindow {
public:
							EditWindow(const char* title,
								const char* section, const FieldSpec* fields,
								int32 fieldCount, const BMessage& record,
								BMessenger target);

	virtual	void			MessageReceived(BMessage* message);

private:
			bool			_Collect(BMessage& record);
			void			_Complain(const char* text, BView* focus);

			BString			fSection;
			const FieldSpec* fFields;
			int32			fFieldCount;
			BMessage		fRecord;
			BMessenger		fTarget;
			std::vector<BView*> fControls;
};

#endif // EDIT_WINDOW_H
