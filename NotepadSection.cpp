/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "NotepadSection.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include <algorithm>
#include <vector>


class NoteItem : public BStringItem {
public:
	NoteItem(const char* title, int64 id)
		:
		BStringItem(title),
		fId(id)
	{
	}

	int64 Id() const { return fId; }

private:
	int64	fId;
};


static bool
NoteLess(const BMessage* a, const BMessage* b)
{
	int result = GetString(*a, "title").ICompare(GetString(*b, "title"));
	if (result != 0)
		return result < 0;
	return GetId(*a) < GetId(*b);
}


NotepadSection::NotepadSection(int32 index, DataStore* store)
	:
	Section(kSectionNames[index], index, store),
	fCurrentId(-1)
{
	fList = new BListView("pages", B_SINGLE_SELECTION_LIST);
	fList->SetSelectionMessage(new BMessage(MSG_LIST_SELECTION));
	BScrollView* listScroll = new BScrollView("pagesScroll", fList, 0,
		false, true);

	fTitle = new BTextControl("title", "Title:", "", NULL);
	fBody = new BTextView("body");
	fBody->SetWordWrap(true);
	fBody->SetInsets(8, 8, 8, 8);
	BScrollView* bodyScroll = new BScrollView("bodyScroll", fBody, 0,
		false, true);

	BStringView* contents = new BStringView("contents", "Contents");
	BFont font(be_bold_font);
	contents->SetFont(&font);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(new SectionHeader(index))
		.AddSplit(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.SetInsets(10, 8, 10, 10)
			.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING, 1)
				.Add(contents)
				.Add(listScroll)
				.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
					.Add(MakeButton("New page", MSG_NEW_ENTRY))
					.Add(MakeButton("Delete", MSG_DELETE_ENTRY))
				.End()
			.End()
			.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING, 3)
				.Add(fTitle)
				.Add(bodyScroll)
			.End()
		.End();

	Reload();
}


void
NotepadSection::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_LIST_SELECTION:
		{
			int64 id = _SelectedId();
			if (id != fCurrentId) {
				Commit();
				_LoadPage(id);
			}
			break;
		}
		default:
			Section::MessageReceived(message);
			break;
	}
}


void
NotepadSection::Reload()
{
	Commit();

	for (int32 i = fList->CountItems() - 1; i >= 0; i--)
		delete fList->RemoveItem(i);

	RecordList& records = fStore->Records(Key());
	std::vector<const BMessage*> pages;
	for (size_t i = 0; i < records.size(); i++)
		pages.push_back(&records[i]);
	std::sort(pages.begin(), pages.end(), NoteLess);

	int32 selectIndex = -1;
	for (size_t i = 0; i < pages.size(); i++) {
		int64 id = GetId(*pages[i]);
		fList->AddItem(new NoteItem(GetString(*pages[i], "title").String(),
			id));
		if (id == fCurrentId)
			selectIndex = i;
	}

	if (selectIndex < 0 && fList->CountItems() > 0)
		selectIndex = 0;

	if (selectIndex >= 0) {
		NoteItem* item = dynamic_cast<NoteItem*>(fList->ItemAt(selectIndex));
		fList->Select(selectIndex);
		_LoadPage(item->Id());
	} else
		_LoadPage(-1);
}


void
NotepadSection::_LoadPage(int64 id)
{
	const BMessage* record = fStore->Find(Key(), id);
	fCurrentId = record != NULL ? id : -1;

	fTitle->SetText(record != NULL
		? GetString(*record, "title").String() : "");
	fBody->SetText(record != NULL
		? GetString(*record, "body").String() : "");
	fTitle->SetEnabled(record != NULL);
	fBody->MakeEditable(record != NULL);
}


int64
NotepadSection::_SelectedId() const
{
	NoteItem* item = dynamic_cast<NoteItem*>(
		fList->ItemAt(fList->CurrentSelection()));
	return item != NULL ? item->Id() : -1;
}


void
NotepadSection::Commit()
{
	if (fCurrentId < 0)
		return;
	const BMessage* record = fStore->Find(Key(), fCurrentId);
	if (record == NULL)
		return;

	BString title(fTitle->Text());
	title.Trim();
	if (title.IsEmpty())
		title = "Untitled page";
	BString body(fBody->Text());

	if (title == GetString(*record, "title")
		&& body == GetString(*record, "body")) {
		return;
	}

	BMessage copy(*record);
	SetString(copy, "title", title.String());
	SetString(copy, "body", body.String());
	fStore->Put(Key(), copy);
	fStore->Save();

	// Keep the table of contents in step without rebuilding it
	for (int32 i = 0; i < fList->CountItems(); i++) {
		NoteItem* item = dynamic_cast<NoteItem*>(fList->ItemAt(i));
		if (item != NULL && item->Id() == fCurrentId) {
			item->SetText(title.String());
			fList->InvalidateItem(i);
			break;
		}
	}
}


void
NotepadSection::NewEntry()
{
	Commit();

	BMessage record;
	SetString(record, "title", "Untitled page");
	SetString(record, "body", "");
	fCurrentId = fStore->Put(Key(), record);
	fStore->Save();
	Reload();

	fTitle->MakeFocus(true);
	if (fTitle->TextView() != NULL)
		fTitle->TextView()->SelectAll();
}


void
NotepadSection::EditSelected()
{
	if (fCurrentId >= 0)
		fBody->MakeFocus(true);
}


void
NotepadSection::DeleteSelected()
{
	const BMessage* record = fStore->Find(Key(), fCurrentId);
	if (record == NULL)
		return;
	if (!ConfirmDelete(GetString(*record, "title").String()))
		return;
	fStore->Remove(Key(), fCurrentId);
	fCurrentId = -1;
	fStore->Save();
	Reload();
}
