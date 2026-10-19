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
#include <string.h>
#include <vector>

#include "DateUtil.h"
#include "Spread.h"

static const float kColumnInset = 8.0f;


static float
ColumnWidth(const ListConfig* config, int32 index, float x, float right)
{
	float width = config->columns[index].width;
	if (index == config->columnCount - 1)
		return std::max(right - x, 10.0f);
	if (width <= 0) {
		// whatever the columns after this one leave
		float rest = 0;
		for (int32 i = index + 1; i < config->columnCount; i++)
			rest += config->columns[i].width;
		width = std::max(right - x - rest, 10.0f);
	}
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

			case COL_SHORT:
			{
				int32 dash = value.FindFirst(" - ");
				if (dash > 0)
					value.Truncate(dash);
				return value;
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


// #pragma mark - IndexTabs


// Index tabs down the outer edge of the left-hand page, like the A-Z
// thumb tabs of a paper address book.
class IndexTabs : public BView {
public:
	IndexTabs(BHandler* target, int32 section,
		const std::vector<BString>& labels)
		:
		BView("indexTabs", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fTarget(target),
		fSection(section),
		fLabels(labels),
		fSelected(0)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
		SetExplicitMinSize(BSize(26, labels.size() * 14));
		SetExplicitMaxSize(BSize(26, B_SIZE_UNLIMITED));
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		SetHighColor(kPaperColor);
		FillRect(bounds);

		BFont font(be_bold_font);
		font.SetSize(10);
		font_height fh;
		font.GetHeight(&fh);

		for (int32 i = 0; i < (int32)fLabels.size(); i++) {
			BRect tab = _TabRect(i);
			bool selected = i == fSelected;
			rgb_color fill = selected
				? kSectionColors[fSection] : kPaperShade;
			SetHighColor(tint_color(fill, B_DARKEN_1_TINT));
			FillRoundRect(tab.OffsetByCopy(-1, 1), 4, 4);
			SetHighColor(fill);
			FillRoundRect(tab, 4, 4);
			// square on the page side
			FillRect(BRect(tab.right - 4, tab.top, tab.right, tab.bottom));

			BString label = fLabels[i];
			BFont labelFont(font);
			float width = labelFont.StringWidth(label.String());
			BPoint start;
			if (width <= tab.Width() - 4) {
				start = BPoint(tab.left + (tab.Width() - width) / 2,
					tab.top + (tab.Height() + fh.ascent - fh.descent) / 2);
			} else {
				// longer labels run up the tab
				labelFont.SetRotation(90);
				labelFont.TruncateString(&label, B_TRUNCATE_END,
					tab.Height() - 6);
				width = labelFont.StringWidth(label.String());
				start = BPoint(
					tab.left + (tab.Width() + fh.ascent - fh.descent) / 2,
					tab.top + (tab.Height() + width) / 2);
			}
			SetFont(&labelFont);
			SetHighColor(kInkColor);
			SetDrawingMode(B_OP_OVER);
			DrawString(label.String(), start);
			SetDrawingMode(B_OP_COPY);
		}
	}

	virtual void MouseDown(BPoint where)
	{
		for (int32 i = 0; i < (int32)fLabels.size(); i++) {
			if (!_TabRect(i).Contains(where))
				continue;
			fSelected = i;
			Invalidate();
			BMessage message(MSG_LETTER);
			message.AddInt32("tab", i);
			BMessenger(fTarget).SendMessage(&message);
			return;
		}
	}

private:
	BRect _TabRect(int32 index) const
	{
		BRect bounds = Bounds();
		float height = floorf(bounds.Height() / fLabels.size());
		float top = bounds.top + index * height;
		return BRect(bounds.left + 2, top + 1, bounds.right,
			top + height - 1);
	}

	BHandler*				fTarget;
	int32					fSection;
	std::vector<BString>	fLabels;
	int32					fSelected;
};


// #pragma mark - DetailCard


// The selected entry, written out on a record card on the right-hand page.
class DetailCard : public BView {
public:
	DetailCard(BHandler* target, const ListConfig* config)
		:
		BView("card", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fTarget(target),
		fConfig(config),
		fHasRecord(false)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
		SetExplicitMinSize(BSize(220, 200));
	}

	void SetRecord(const BMessage* record, const char* emptyText)
	{
		fHasRecord = record != NULL;
		if (record != NULL)
			fRecord = *record;
		fEmptyText = emptyText;
		Invalidate();
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		SetHighColor(kPaperColor);
		FillRect(bounds);

		BFont plain(be_plain_font);
		plain.SetSize(12);
		if (!fHasRecord) {
			SetFont(&plain);
			SetLowColor(kPaperColor);
			SetHighColor(150, 146, 130);
			std::vector<BString> lines;
			_Wrap(fEmptyText, plain, bounds.Width() - 60, lines);
			float y = floorf(bounds.top + bounds.Height() / 3);
			for (size_t i = 0; i < lines.size(); i++) {
				float width = plain.StringWidth(lines[i].String());
				DrawString(lines[i].String(), BPoint(
					floorf(bounds.left + (bounds.Width() - width) / 2), y));
				y += 18;
			}
			return;
		}

		BRect card = bounds.InsetByCopy(18, 16);
		BFont bold(be_bold_font);
		bold.SetSize(17);
		BFont label(be_plain_font);
		label.SetSize(10);
		font_height plainHeight, boldHeight, labelHeight;
		plain.GetHeight(&plainHeight);
		bold.GetHeight(&boldHeight);
		label.GetHeight(&labelHeight);
		float textWidth = card.Width() - 28;

		// Lay the card out first, so its border fits the text.
		std::vector<Row> rows;
		float height = 18 + boldHeight.ascent + 14;
		for (int32 i = 0; i < fConfig->fieldCount; i++) {
			const FieldSpec& field = fConfig->fields[i];
			if (_IsTitleKey(field.key))
				continue;
			BString value = _Value(field);
			if (value.IsEmpty())
				continue;
			Row row;
			row.label = field.label;
			_Wrap(value, plain, textWidth, row.lines);
			rows.push_back(row);
			height += labelHeight.ascent + 4
				+ row.lines.size() * (plainHeight.ascent
					+ plainHeight.descent + 3) + 8;
		}
		card.bottom = std::min(card.bottom, card.top + height + 8);

		// the card, with a soft shadow and a coloured top edge
		SetHighColor(kPaperShade);
		FillRect(card.OffsetByCopy(3, 3));
		SetHighColor(255, 255, 250);
		FillRect(card);
		SetHighColor(kSectionColors[_Section()]);
		FillRect(BRect(card.left, card.top, card.right, card.top + 4));
		SetHighColor(tint_color(kPaperShade, B_DARKEN_1_TINT));
		StrokeRect(card);

		float x = card.left + 14;
		float y = card.top + 18 + boldHeight.ascent;
		SetLowColor(255, 255, 250);
		SetFont(&bold);
		SetHighColor(kInkColor);
		BString title = _Title();
		bold.TruncateString(&title, B_TRUNCATE_END, textWidth);
		DrawString(title.String(), BPoint(x, y));
		y += 14;

		for (size_t i = 0; i < rows.size() && y < card.bottom; i++) {
			y += labelHeight.ascent + 4;
			SetFont(&label);
			SetHighColor(140, 136, 120);
			BString text(rows[i].label);
			text.ToUpper();
			DrawString(text.String(), BPoint(x, y));
			SetFont(&plain);
			SetHighColor(kInkColor);
			for (size_t j = 0; j < rows[i].lines.size(); j++) {
				y += plainHeight.ascent + plainHeight.descent + 3;
				DrawString(rows[i].lines[j].String(), BPoint(x, y));
			}
			y += 8;
		}

		SetFont(&label);
		SetLowColor(kPaperColor);
		SetHighColor(170, 166, 150);
		const char* hint = "Double-click the card to edit";
		DrawString(hint, BPoint(
			bounds.right - 18 - label.StringWidth(hint), bounds.bottom - 8));
	}

	virtual void MouseDown(BPoint where)
	{
		int32 clicks = 1;
		if (Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("clicks", &clicks);
		if (fHasRecord && clicks >= 2)
			BMessenger(fTarget).SendMessage(MSG_EDIT_ENTRY);
	}

private:
	struct Row {
		const char*				label;
		std::vector<BString>	lines;
	};

	int32 _Section() const
	{
		Section* section = dynamic_cast<Section*>(fTarget);
		return section != NULL ? section->Index() : 0;
	}

	bool _IsTitleKey(const char* key) const
	{
		for (int i = 0; i < 3 && fConfig->cardTitleKeys[i] != NULL; i++) {
			if (strcmp(key, fConfig->cardTitleKeys[i]) == 0)
				return true;
		}
		return false;
	}

	BString _Title() const
	{
		BString title;
		for (int i = 0; i < 3 && fConfig->cardTitleKeys[i] != NULL; i++) {
			BString part = GetString(fRecord, fConfig->cardTitleKeys[i]);
			if (part.IsEmpty())
				continue;
			if (!title.IsEmpty())
				title << " ";
			title << part;
		}
		if (title.IsEmpty())
			title = GetString(fRecord, "company");
		if (title.IsEmpty())
			title << "(untitled " << fConfig->singular << ")";
		return title;
	}

	BString _Value(const FieldSpec& field) const
	{
		BString value = GetString(fRecord, field.key);
		SimpleDate date;
		switch (field.type) {
			case FIELD_CHECK:
				return GetBool(fRecord, field.key)
					? BString("\xE2\x9C\x93 Yes") : BString();
			case FIELD_DATE:
				if (ParseDate(value.String(), date))
					return FormatLong(date);
				return value;
			default:
				return value;
		}
	}

	static void _Wrap(const BString& text, const BFont& font, float width,
		std::vector<BString>& lines)
	{
		BString rest(text);
		while (true) {
			BString paragraph;
			int32 newline = rest.FindFirst('\n');
			if (newline < 0) {
				paragraph = rest;
				rest = "";
			} else {
				rest.CopyInto(paragraph, 0, newline);
				rest.Remove(0, newline + 1);
			}

			BString line;
			int32 start = 0;
			while (start <= paragraph.Length()) {
				int32 space = paragraph.FindFirst(' ', start);
				int32 end = space < 0 ? paragraph.Length() : space;
				BString word;
				paragraph.CopyInto(word, start, end - start);
				BString candidate(line);
				if (!candidate.IsEmpty())
					candidate << " ";
				candidate << word;
				if (!line.IsEmpty()
					&& font.StringWidth(candidate.String()) > width) {
					lines.push_back(line);
					line = word;
				} else
					line = candidate;
				if (space < 0)
					break;
				start = space + 1;
			}
			lines.push_back(line);
			if (newline < 0)
				break;
		}
	}

	BHandler*			fTarget;
	const ListConfig*	fConfig;
	BMessage			fRecord;
	bool				fHasRecord;
	BString				fEmptyText;
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


static const char* kTodoTabs[] = {
	"All", "Overdue", "Current", "Future", "Completed"
};
static const int32 kTodoTabCount = 5;

// To Do tabs: a task is current until a week before it is due
static const int kFutureDays = 7;


RecordListSection::RecordListSection(int32 index, DataStore* store,
	const ListConfig& config)
	:
	Section(kSectionNames[index], index, store),
	fConfig(config),
	fTabs(NULL),
	fTab(0),
	fVisible(0)
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
	fList->SetSelectionMessage(new BMessage(MSG_LIST_SELECTION));
	fList->SetInvocationMessage(new BMessage(MSG_EDIT_ENTRY));
	BScrollView* scroll = new BScrollView("scroll", fList, 0, false, true,
		B_NO_BORDER);

	std::vector<BString> labels;
	if (config.index == INDEX_LETTERS) {
		labels.push_back("All");
		for (char letter = 'A'; letter <= 'Z'; letter++)
			labels.push_back(BString() << letter);
	} else if (config.index == INDEX_TODO) {
		for (int32 i = 0; i < kTodoTabCount; i++)
			labels.push_back(kTodoTabs[i]);
	}

	fCard = new DetailCard(this, &fConfig);

	SpreadView* spread = new SpreadView();
	BGroupLayout* left = new BGroupLayout(B_HORIZONTAL, 0);
	spread->LeftPage()->GroupLayout()->AddItem(left);
	if (!labels.empty()) {
		fTabs = new IndexTabs(this, index, labels);
		left->AddView(fTabs);
	}
	BLayoutBuilder::Group<>(left)
		.AddGroup(B_VERTICAL, 0)
			.Add(new ListHeader(&fConfig))
			.Add(scroll)
		.End();
	spread->RightPage()->GroupLayout()->AddView(fCard);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(new SectionHeader(index))
		.Add(toolbar)
		.Add(spread);

	Reload();
}


void
RecordListSection::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_LETTER:
		{
			int32 tab;
			if (message->FindInt32("tab", &tab) == B_OK) {
				fTab = tab;
				Reload();
			}
			break;
		}
		case MSG_LIST_SELECTION:
			_UpdateCard();
			break;
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
	fVisible = visible.size();

	int32 selectIndex = -1;
	for (size_t i = 0; i < visible.size(); i++) {
		fList->AddItem(new RecordItem(*visible[i], &fConfig));
		if (GetId(*visible[i]) == selectedId)
			selectIndex = i;
	}
	// keep a card on the right-hand page whenever there is something
	if (selectIndex < 0 && !visible.empty())
		selectIndex = 0;
	if (selectIndex >= 0)
		fList->Select(selectIndex);

	BString count;
	if (fTab == 0) {
		count.SetToFormat("%d %s%s", (int)records.size(),
			fConfig.singular, records.size() == 1 ? "" : "s");
	} else {
		count.SetToFormat("%d of %d shown", (int)visible.size(),
			(int)records.size());
	}
	count.ToLower();
	fCount->SetText(count.String());

	_UpdateCard();
}


void
RecordListSection::_UpdateCard()
{
	BString empty;
	BString singular(fConfig.singular);
	singular.ToLower();
	if (fStore->Records(Key()).empty()) {
		empty.SetToFormat("No %ss yet.\nClick \"New %s\" to add one.",
			singular.String(), fConfig.singular);
	} else if (fVisible == 0)
		empty.SetToFormat("No %ss behind this tab.", singular.String());
	else
		empty.SetToFormat("Select a %s to see it here.", singular.String());
	fCard->SetRecord(_SelectedRecord(), empty.String());
}


bool
RecordListSection::_Matches(const BMessage& record) const
{
	if (fTab == 0)
		return true;

	if (fConfig.index == INDEX_LETTERS) {
		BString value = GetString(record, fConfig.letterKey);
		if (value.IsEmpty())
			value = GetString(record, "company");
		return !value.IsEmpty()
			&& toupper((unsigned char)value[0]) == 'A' + fTab - 1;
	}

	if (fConfig.index == INDEX_TODO) {
		bool done = GetBool(record, fConfig.doneKey);
		if (fTab == 4)
			return done;
		if (done)
			return false;
		SimpleDate due;
		bool hasDue = ParseDate(GetString(record, "due").String(), due);
		long today = DayNumber(Today());
		bool overdue = hasDue && DayNumber(due) < today;
		bool future = hasDue && DayNumber(due) > today + kFutureDays;
		switch (fTab) {
			case 1:
				return overdue;
			case 2:
				return !overdue && !future;
			case 3:
				return future;
		}
	}
	return true;
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
