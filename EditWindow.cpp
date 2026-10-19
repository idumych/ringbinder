/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "EditWindow.h"

#include <Alert.h>
#include <Button.h>
#include <CheckBox.h>
#include <GridLayout.h>
#include <GridView.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include "Constants.h"
#include "DataStore.h"
#include "DateUtil.h"


EditWindow::EditWindow(const char* title, const char* section,
	const FieldSpec* fields, int32 fieldCount, const BMessage& record,
	BMessenger target)
	:
	BWindow(BRect(0, 0, 440, 200), title, B_TITLED_WINDOW,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS
			| B_CLOSE_ON_ESCAPE),
	fSection(section),
	fFields(fields),
	fFieldCount(fieldCount),
	fRecord(record),
	fTarget(target)
{
	BGridView* gridView = new BGridView(B_USE_DEFAULT_SPACING,
		B_USE_SMALL_SPACING);
	BGridLayout* grid = gridView->GridLayout();
	grid->SetColumnWeight(1, 1.0f);

	BView* firstFocus = NULL;

	for (int32 row = 0; row < fieldCount; row++) {
		const FieldSpec& field = fields[row];
		BString value = GetString(record, field.key);
		BView* control = NULL;

		BString labelText(field.label);
		if (field.required)
			labelText << " *";
		labelText << ":";

		switch (field.type) {
			case FIELD_CHECK:
			{
				BCheckBox* box = new BCheckBox(field.key, field.label, NULL);
				box->SetValue(GetBool(record, field.key)
					? B_CONTROL_ON : B_CONTROL_OFF);
				grid->AddView(box, 1, row);
				control = box;
				break;
			}

			case FIELD_CHOICE:
			{
				BPopUpMenu* menu = new BPopUpMenu(field.key);
				BString choices(field.choices);
				int32 start = 0;
				while (start <= choices.Length()) {
					int32 bar = choices.FindFirst('|', start);
					if (bar < 0)
						bar = choices.Length();
					BString item;
					choices.CopyInto(item, start, bar - start);
					BMenuItem* menuItem = new BMenuItem(item.String(), NULL);
					menu->AddItem(menuItem);
					if (item == value)
						menuItem->SetMarked(true);
					start = bar + 1;
				}
				if (menu->FindMarked() == NULL && menu->ItemAt(0) != NULL)
					menu->ItemAt(0)->SetMarked(true);

				BMenuField* menuField = new BMenuField(field.key, NULL, menu);
				grid->AddView(new BStringView("label", labelText.String()),
					0, row);
				grid->AddView(menuField, 1, row);
				control = menuField;
				break;
			}

			case FIELD_MULTILINE:
			{
				BTextView* textView = new BTextView(field.key);
				textView->SetWordWrap(true);
				textView->SetText(value.String());
				textView->SetExplicitMinSize(BSize(320, 90));
				BScrollView* scroll = new BScrollView("scroll", textView, 0,
					false, true);
				BStringView* label = new BStringView("label",
					labelText.String());
				label->SetExplicitAlignment(
					BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));
				grid->AddView(label, 0, row);
				grid->AddView(scroll, 1, row);
				control = textView;
				break;
			}

			case FIELD_TEXT:
			case FIELD_DATE:
			case FIELD_TIME:
			default:
			{
				BTextControl* text = new BTextControl(field.key, NULL,
					value.String(), NULL);
				if (field.type == FIELD_DATE)
					text->SetToolTip("YYYY-MM-DD, or \"today\"");
				else if (field.type == FIELD_TIME)
					text->SetToolTip("HH:MM (24 hour)");
				grid->AddView(new BStringView("label", labelText.String()),
					0, row);
				grid->AddView(text, 1, row);
				control = text;
				if (firstFocus == NULL)
					firstFocus = text;
				break;
			}
		}

		fControls.push_back(control);
	}

	BButton* cancel = new BButton("cancel", "Cancel",
		new BMessage(B_QUIT_REQUESTED));
	BButton* save = new BButton("save", "Save", new BMessage(MSG_EDIT_OK));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(gridView)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(cancel)
			.Add(save)
		.End();

	SetDefaultButton(save);
	if (firstFocus != NULL)
		firstFocus->MakeFocus(true);

	ResizeToPreferred();
	CenterOnScreen();
}


void
EditWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_EDIT_OK:
		{
			BMessage record;
			if (!_Collect(record))
				break;

			BMessage reply(MSG_RECORD_SAVED);
			reply.AddString("section", fSection.String());
			reply.AddMessage("record", &record);
			fTarget.SendMessage(&reply);
			Quit();
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
EditWindow::_Collect(BMessage& record)
{
	record = fRecord;

	for (int32 i = 0; i < fFieldCount; i++) {
		const FieldSpec& field = fFields[i];
		BView* control = fControls[i];
		BString value;

		switch (field.type) {
			case FIELD_CHECK:
			{
				BCheckBox* box = dynamic_cast<BCheckBox*>(control);
				SetBool(record, field.key,
					box != NULL && box->Value() == B_CONTROL_ON);
				continue;
			}

			case FIELD_CHOICE:
			{
				BMenuField* menuField = dynamic_cast<BMenuField*>(control);
				BMenuItem* marked = menuField != NULL
					? menuField->Menu()->FindMarked() : NULL;
				if (marked != NULL)
					value = marked->Label();
				break;
			}

			case FIELD_MULTILINE:
			{
				BTextView* textView = dynamic_cast<BTextView*>(control);
				if (textView != NULL)
					value = textView->Text();
				break;
			}

			default:
			{
				BTextControl* text = dynamic_cast<BTextControl*>(control);
				if (text != NULL)
					value = text->Text();
				value.Trim();

				if (field.type == FIELD_DATE && !value.IsEmpty()) {
					SimpleDate date;
					if (!ParseDate(value.String(), date)) {
						BString error;
						error.SetToFormat("\"%s\" is not a valid date.\n\n"
							"Please use the form YYYY-MM-DD, for example %s.",
							value.String(), FormatDate(Today()).String());
						_Complain(error.String(), text);
						return false;
					}
					value = FormatDate(date);
				} else if (field.type == FIELD_TIME && !value.IsEmpty()) {
					int hour, minute;
					if (!ParseTime(value.String(), hour, minute)) {
						BString error;
						error.SetToFormat("\"%s\" is not a valid time.\n\n"
							"Please use 24-hour HH:MM, for example 14:30.",
							value.String());
						_Complain(error.String(), text);
						return false;
					}
					value = FormatTime(hour, minute);
				}
				break;
			}
		}

		if (field.required && value.IsEmpty()) {
			BString error;
			error.SetToFormat("Please fill in \"%s\".", field.label);
			_Complain(error.String(), control);
			return false;
		}

		SetString(record, field.key, value.String());
	}
	return true;
}


void
EditWindow::_Complain(const char* text, BView* focus)
{
	BAlert* alert = new BAlert(APP_NAME, text, "OK", NULL, NULL,
		B_WIDTH_AS_USUAL, B_STOP_ALERT);
	alert->Go();
	if (focus != NULL)
		focus->MakeFocus(true);
}
