/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "Section.h"

#include <Alert.h>
#include <Button.h>
#include <Font.h>
#include <Invoker.h>
#include <Window.h>

#include "DateUtil.h"


SectionHeader::SectionHeader(int32 index)
	:
	BView("header", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fIndex(index)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	SetExplicitMinSize(BSize(B_SIZE_UNSET, 40));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 40));
}


void
SectionHeader::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

	SetHighColor(kPaperColor);
	FillRect(bounds);
	SetHighColor(kSectionColors[fIndex]);
	FillRect(BRect(bounds.left, bounds.bottom - 4, bounds.right,
		bounds.bottom));

	SetLowColor(kPaperColor);
	SetHighColor(kInkColor);

	BFont font(be_bold_font);
	font.SetSize(20);
	SetFont(&font);
	DrawString(kSectionNames[fIndex],
		BPoint(bounds.left + 14, bounds.bottom - 13));

	BFont small(be_plain_font);
	small.SetSize(12);
	SetFont(&small);
	BString today = FormatLong(Today());
	DrawString(today.String(), BPoint(
		bounds.right - small.StringWidth(today.String()) - 14,
		bounds.bottom - 14));
}


// #pragma mark - Section


static void
RetargetChildren(BView* parent, BHandler* target)
{
	for (int32 i = 0; BView* child = parent->ChildAt(i); i++) {
		BInvoker* invoker = dynamic_cast<BInvoker*>(child);
		if (invoker != NULL)
			invoker->SetTarget(target);
		RetargetChildren(child, target);
	}
}


Section::Section(const char* name, int32 index, DataStore* store)
	:
	BView(name, B_WILL_DRAW),
	fStore(store),
	fIndex(index)
{
	SetViewColor(kPaperColor);
}


void
Section::AllAttached()
{
	BView::AllAttached();
	// Buttons and lists inside this page talk to the page itself
	RetargetChildren(this, this);
}


void
Section::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_NEW_ENTRY:
			NewEntry();
			break;
		case MSG_EDIT_ENTRY:
			EditSelected();
			break;
		case MSG_DELETE_ENTRY:
			DeleteSelected();
			break;
		case MSG_TOGGLE_DONE:
			ToggleSelected();
			break;
		default:
			BView::MessageReceived(message);
			break;
	}
}


void
Section::OpenEditor(const char* title, const FieldSpec* fields, int32 count,
	const BMessage& record)
{
	EditWindow* window = new EditWindow(title, Key(), fields, count, record,
		BMessenger(Window()));
	window->Show();
}


bool
Section::ConfirmDelete(const char* what)
{
	BString text;
	text.SetToFormat("Delete \"%s\"?\n\nThis cannot be undone.", what);
	BAlert* alert = new BAlert("Delete", text.String(), "Cancel", "Delete",
		NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	return alert->Go() == 1;
}


void
Section::DataChanged()
{
	fStore->Save();
	if (Window() != NULL)
		Window()->PostMessage(MSG_DATA_CHANGED);
}


BButton*
Section::MakeButton(const char* label, uint32 what)
{
	return new BButton(label, new BMessage(what));
}
