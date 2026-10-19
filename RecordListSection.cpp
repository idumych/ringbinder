/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "RecordListSection.h"

#include <Button.h>
#include <GroupLayout.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <Messenger.h>
#include <ScrollView.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>
#include <Window.h>

#include <algorithm>
#include <ctype.h>

#include "DateUtil.h"

static const float kColumnInset = 8.0f;


static float
ColumnWidth(const ListConfig* config, int32 index, float x, float right)
{
	float width = config->columns[index].width;
	if (width <= 0 || index == config->columnCount - 1)
		width = std::max(right - x, 10.0f);
	return width;
}


// #pragma mark - RecordItem


class RecordItem : public BListItem {
public:
	RecordItem(const BMessage& record, const ListConfig* config)
		:
		fRecord(record),
		fConfig(config)
	{
	}

	int64 Id() const { return GetId(fRecord); }

	virtual void Update(BView* owner, const BFont* font)
	{
		BListItem::Update(owner, font);
		SetHeight(Height() + 8);
	}

	virtual void DrawItem(BView* owner, BRect frame, bool complete)
	{
		bool done = fConfig->doneKey != NULL
			&& GetBool(fRecord, fConfig->doneKey);

		rgb_color background = IsSelected()
			? ui_color(B_LIST_SELECTED_BACKGROUND_COLOR) : kPaperColor;
		rgb_color ink = IsSelected()
			? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR) : kInkColor;
		if (done && !IsSelected())
			ink = make_color(150, 150, 150);

		owner->SetHighColor(background);
		owner->FillRect(frame);
		owner->SetHighColor(kRuleColor);
		owner->StrokeLine(frame.LeftBottom(), frame.RightBottom());

		font_height fh;
		owner->GetFontHeight(&fh);
		float baseline = floorf(frame.top
			+ (frame.Height() - (fh.ascent + fh.descent)) / 2 + fh.ascent);

		owner->SetLowColor(background);
		owner->SetHighColor(ink);

		float x = frame.left + kColumnInset;
		for (int32 i = 0; i < fConfig->columnCount; i++) {
			float width = ColumnWidth(fConfig, i, x, frame.right);
			BString text = _ColumnText(fConfig->columns[i]);
			owner->TruncateString(&text, B_TRUNCATE_END, width - 8);
			owner->DrawString(text.String(), BPoint(x, baseline));

			if (done && fConfig->columns[i].format != COL_CHECK
				&& !text.IsEmpty()) {
				float strikeY = baseline - floorf(fh.ascent / 3);
				owner->StrokeLine(BPoint(x, strikeY),
					BPoint(x + owner->StringWidth(text.String()), strikeY));
			}
			x += width;
		}
	}

private:
	BString _ColumnText(const ColumnSpec& column) const
	{
		BString value = GetString(fRecord, column.key);
		SimpleDate date;

		switch (column.format) {
			case COL_CHECK:
				return GetBool(fRecord, column.key)
					? BString("\xE2\x9C\x93") : BString();

			case COL_DATE:
				if (ParseDate(value.String(), date))
					return FormatShort(date);
				return value;

			case COL_MONTHDAY:
				if (ParseDate(value.String(), date))
					return FormatMonthDay(date);
				return value;

			case COL_COUNTDOWN:
			{
				if (!ParseDate(value.String(), date))
					return BString();
				int days = DaysUntilNext(date);
				if (days == 0)
					return BString("Today!");
				if (days == 1)
					return BString("Tomorrow");
				BString text;
				text.SetToFormat("in %d days", days);
				return text;
			}

			default:
				value.ReplaceAll('\n', ' ');
				return value;
		}
	}

	BMessage			fRecord;
	const ListConfig*	fConfig;
};


// #pragma mark - ListHeader


class ListHeader : public BView {
public:
	ListHeader(const ListConfig* config)
		:
		BView("listHeader", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fConfig(config)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
		SetExplicitMinSize(BSize(B_SIZE_UNSET, 22));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 22));
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		SetHighColor(kPaperShade);
		FillRect(bounds);
		SetHighColor(tint_color(kPaperShade, B_DARKEN_2_TINT));
		StrokeLine(bounds.LeftBottom(), bounds.RightBottom());

		BFont font(be_bold_font);
		font.SetSize(11);
		SetFont(&font);
		SetLowColor(kPaperShade);
		SetHighColor(kInkColor);

		float x = bounds.left + kColumnInset;
		// leave room for the scroll bar so columns line up with the list
		float right = bounds.right - B_V_SCROLL_BAR_WIDTH;
		for (int32 i = 0; i < fConfig->columnCount; i++) {
			float width = ColumnWidth(fConfig, i, x, right);
			BString title(fConfig->columns[i].title);
			TruncateString(&title, B_TRUNCATE_END, width - 8);
			DrawString(title.String(), BPoint(x, bounds.bottom - 6));
			x += width;
		}
	}

private:
	const ListConfig*	fConfig;
};


// #pragma mark - LetterBar


class LetterBar : public BView {
public:
	LetterBar(BHandler* target)
		:
		BView("letters", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fTarget(target),
		fSelected(0)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
		SetExplicitMinSize(BSize(300, 24));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 24));
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		SetHighColor(kPaperColor);
		FillRect(bounds);

		BFont font(be_bold_font);
		font.SetSize(11);
		SetFont(&font);

		for (int32 i = 0; i < 27; i++) {
			BRect cell = _CellRect(i);
			bool selected = i == fSelected;
			rgb_color fill = selected
				? kSectionColors[SECTION_ADDRESS] : kPaperShade;
			SetHighColor(fill);
			FillRoundRect(cell.InsetByCopy(1, 2), 3, 3);
			SetLowColor(fill);
			SetHighColor(kInkColor);

			BString label = _Label(i);
			float width = StringWidth(label.String());
			DrawString(label.String(), BPoint(
				cell.left + (cell.Width() - width) / 2, cell.bottom - 7));
		}
	}

	virtual void MouseDown(BPoint where)
	{
		for (int32 i = 0; i < 27; i++) {
			if (!_CellRect(i).Contains(where))
				continue;
			fSelected = i;
			Invalidate();
			BMessage message(MSG_LETTER);
			message.AddString("letter",
				i == 0 ? "" : _Label(i).String());
			BMessenger(fTarget).SendMessage(&message);
			return;
		}
	}

private:
	BRect _CellRect(int32 index) const
	{
		// "All" is twice as wide as the letters
		BRect bounds = Bounds().InsetByCopy(8, 0);
		float unit = bounds.Width() / 28;
		float left = index == 0 ? bounds.left
			: bounds.left + (index + 1) * unit;
		float width = index == 0 ? unit * 2 : unit;
		return BRect(left, bounds.top, left + width - 1, bounds.bottom);
	}

	BString _Label(int32 index) const
	{
		if (index == 0)
			return BString("All");
		BString letter;
		letter << (char)('A' + index - 1);
		return letter;
	}

	BHandler*	fTarget;
	int32		fSelected;
};


// #pragma mark - sorting


struct RecordLess {
	const ListConfig* config;

	bool operator()(const BMessage* a, const BMessage* b) const
	{
		if (config->doneKey != NULL) {
			bool doneA = GetBool(*a, config->doneKey);
			bool doneB = GetBool(*b, config->doneKey);
			if (doneA != doneB)
				return !doneA;
		}
		if (config->monthDaySort) {
			int keyA = _MonthDayKey(*a);
			int keyB = _MonthDayKey(*b);
			if (keyA != keyB)
				return keyA < keyB;
		}
		for (int i = 0; i < 4 && config->sortKeys[i] != NULL; i++) {
			BString valueA = GetString(*a, config->sortKeys[i]);
			BString valueB = GetString(*b, config->sortKeys[i]);
			// empty values go to the end
			if (valueA.IsEmpty() != valueB.IsEmpty())
				return valueB.IsEmpty();
			int result = valueA.ICompare(valueB);
			if (result != 0)
				return result < 0;
		}
		return GetId(*a) < GetId(*b);
	}

	static int _MonthDayKey(const BMessage& record)
	{
		SimpleDate date;
		if (!ParseDate(GetString(record, "date").String(), date))
			return 9999;
		return date.month * 100 + date.day;
	}
};


// #pragma mark - RecordListSection


RecordListSection::RecordListSection(int32 index, DataStore* store,
	const ListConfig& config)
	:
	Section(kSectionNames[index], index, store),
	fConfig(config),
	fLetterBar(NULL)
{
	BGroupView* toolbar = new BGroupView(B_HORIZONTAL, 6);
	toolbar->SetViewColor(kPaperColor);
	BGroupLayout* toolLayout = toolbar->GroupLayout();
	toolLayout->SetInsets(10, 6, 10, 6);

	BString newLabel("New ");
	newLabel << config.singular;
	toolLayout->AddView(MakeButton(newLabel.String(), MSG_NEW_ENTRY));
	toolLayout->AddView(MakeButton("Edit", MSG_EDIT_ENTRY));
	toolLayout->AddView(MakeButton("Delete", MSG_DELETE_ENTRY));
	if (config.doneKey != NULL)
		toolLayout->AddView(MakeButton("Mark done", MSG_TOGGLE_DONE));
	toolLayout->AddItem(BSpaceLayoutItem::CreateGlue());
	fCount = new BStringView("count", "");
	toolLayout->AddView(fCount);

	fList = new BListView("list", B_SINGLE_SELECTION_LIST);
	fList->SetViewColor(kPaperColor);
	fList->SetLowColor(kPaperColor);
	fList->SetInvocationMessage(new BMessage(MSG_EDIT_ENTRY));
	BScrollView* scroll = new BScrollView("scroll", fList, 0, false, true,
		B_NO_BORDER);

	BLayoutBuilder::Group<> builder(this, B_VERTICAL, 0);
	builder.Add(new SectionHeader(index)).Add(toolbar);
	if (config.letterKey != NULL) {
		fLetterBar = new LetterBar(this);
		builder.Add(fLetterBar);
	}
	builder.Add(new ListHeader(&fConfig)).Add(scroll);

	Reload();
}


void
RecordListSection::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_LETTER:
		{
			const char* letter;
			if (message->FindString("letter", &letter) == B_OK) {
				fLetter = letter;
				Reload();
			}
			break;
		}
		default:
			Section::MessageReceived(message);
			break;
	}
}


void
RecordListSection::Reload()
{
	int64 selectedId = -1;
	RecordItem* selected = dynamic_cast<RecordItem*>(
		fList->ItemAt(fList->CurrentSelection()));
	if (selected != NULL)
		selectedId = selected->Id();

	for (int32 i = fList->CountItems() - 1; i >= 0; i--)
		delete fList->RemoveItem(i);

	RecordList& records = fStore->Records(Key());
	std::vector<const BMessage*> visible;
	for (size_t i = 0; i < records.size(); i++) {
		if (_Matches(records[i]))
			visible.push_back(&records[i]);
	}
	RecordLess less = { &fConfig };
	std::sort(visible.begin(), visible.end(), less);

	int32 selectIndex = -1;
	for (size_t i = 0; i < visible.size(); i++) {
		fList->AddItem(new RecordItem(*visible[i], &fConfig));
		if (GetId(*visible[i]) == selectedId)
			selectIndex = i;
	}
	if (selectIndex >= 0)
		fList->Select(selectIndex);

	BString count;
	if (fLetter.IsEmpty()) {
		count.SetToFormat("%d %s%s", (int)records.size(),
			fConfig.singular, records.size() == 1 ? "" : "s");
	} else {
		count.SetToFormat("%d of %d shown", (int)visible.size(),
			(int)records.size());
	}
	count.ToLower();
	fCount->SetText(count.String());
}


bool
RecordListSection::_Matches(const BMessage& record) const
{
	if (fConfig.letterKey == NULL || fLetter.IsEmpty())
		return true;
	BString value = GetString(record, fConfig.letterKey);
	if (value.IsEmpty())
		value = GetString(record, "company");
	return !value.IsEmpty()
		&& toupper((unsigned char)value[0]) == fLetter[0];
}


const BMessage*
RecordListSection::_SelectedRecord()
{
	RecordItem* item = dynamic_cast<RecordItem*>(
		fList->ItemAt(fList->CurrentSelection()));
	if (item == NULL)
		return NULL;
	return fStore->Find(Key(), item->Id());
}


void
RecordListSection::NewEntry()
{
	BMessage record;
	if (fConfig.defaults != NULL)
		fConfig.defaults(record);
	BString title("New ");
	title << fConfig.singular;
	OpenEditor(title.String(), fConfig.fields, fConfig.fieldCount, record);
}


void
RecordListSection::EditSelected()
{
	const BMessage* record = _SelectedRecord();
	if (record == NULL)
		return;
	BString title("Edit ");
	title << fConfig.singular;
	OpenEditor(title.String(), fConfig.fields, fConfig.fieldCount, *record);
}


void
RecordListSection::DeleteSelected()
{
	const BMessage* record = _SelectedRecord();
	if (record == NULL)
		return;
	int64 id = GetId(*record);
	BString what = GetString(*record, fConfig.titleKey);
	if (what.IsEmpty())
		what = fConfig.singular;
	if (!ConfirmDelete(what.String()))
		return;
	fStore->Remove(Key(), id);
	DataChanged();
}


void
RecordListSection::ToggleSelected()
{
	if (fConfig.doneKey == NULL)
		return;
	const BMessage* record = _SelectedRecord();
	if (record == NULL)
		return;
	BMessage copy(*record);
	SetBool(copy, fConfig.doneKey, !GetBool(copy, fConfig.doneKey));
	fStore->Put(Key(), copy);
	DataChanged();
}
