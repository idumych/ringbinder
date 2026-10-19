/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "PlannerSection.h"

#include <Button.h>
#include <Font.h>
#include <GroupLayout.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <Messenger.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>
#include <Window.h>

#include <algorithm>
#include <math.h>


static const char* kPlannerCategories[] = {
	"Meeting", "Vacation", "Travel", "Training", "Holiday", "Other"
};
static const rgb_color kPlannerColors[] = {
	{ 110, 150, 220, 255 },
	{ 120, 190, 110, 255 },
	{ 230, 160,  80, 255 },
	{ 180, 120, 200, 255 },
	{ 220, 100, 100, 255 },
	{ 160, 160, 160, 255 }
};
static const int kPlannerCategoryCount = 6;

static const FieldSpec kPlannerFields[] = {
	{ "title",		"Event",		FIELD_TEXT,		NULL, true },
	{ "category",	"Category",		FIELD_CHOICE,
		"Meeting|Vacation|Travel|Training|Holiday|Other", false },
	{ "start",		"Start date",	FIELD_DATE,		NULL, true },
	{ "end",		"End date",		FIELD_DATE,		NULL, true },
	{ "notes",		"Notes",		FIELD_MULTILINE, NULL, false }
};
static const int32 kPlannerFieldCount
	= sizeof(kPlannerFields) / sizeof(kPlannerFields[0]);

static const float kMonthColumn = 86;
static const float kTopRow = 22;
static const float kLegendHeight = 28;


static rgb_color
CategoryColor(const BString& category)
{
	for (int i = 0; i < kPlannerCategoryCount; i++) {
		if (category == kPlannerCategories[i])
			return kPlannerColors[i];
	}
	return kPlannerColors[kPlannerCategoryCount - 1];
}


static bool
EventRange(const BMessage& record, long& start, long& end)
{
	SimpleDate first, last;
	if (!ParseDate(GetString(record, "start").String(), first))
		return false;
	if (!ParseDate(GetString(record, "end").String(), last))
		last = first;
	start = DayNumber(first);
	end = DayNumber(last);
	if (end < start)
		std::swap(start, end);
	return true;
}


// #pragma mark - YearView


class YearView : public BView {
public:
	YearView(DataStore* store, BHandler* owner)
		:
		BView("year", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fStore(store),
		fOwner(owner),
		fYear(Today().year),
		fSelectedId(-1),
		fDragging(false)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
		SetExplicitMinSize(BSize(kMonthColumn + 31 * 16 + 16, 360));
	}

	int Year() const { return fYear; }
	int64 SelectedId() const { return fSelectedId; }

	void SetYear(int year)
	{
		fYear = year;
		fSelectedId = -1;
		Invalidate();
	}

	void ClearSelection()
	{
		fSelectedId = -1;
		Invalidate();
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		SetHighColor(kPaperColor);
		FillRect(bounds);

		BFont small(be_plain_font);
		small.SetSize(10);
		BFont bold(be_bold_font);
		bold.SetSize(12);
		font_height fh;
		bold.GetHeight(&fh);

		// day numbers along the top
		SetFont(&small);
		SetLowColor(kPaperColor);
		SetHighColor(kInkColor);
		for (int day = 1; day <= 31; day++) {
			BRect cell = _CellRect(1, day);
			BString number;
			number << day;
			float width = StringWidth(number.String());
			DrawString(number.String(), BPoint(
				cell.left + (cell.Width() - width) / 2, kTopRow - 6));
		}

		SimpleDate today = Today();
		rgb_color weekend = make_color(236, 230, 210);
		rgb_color invalid = make_color(205, 198, 182);

		for (int month = 1; month <= 12; month++) {
			BRect row = _CellRect(month, 1);
			SetFont(&bold);
			SetHighColor(kInkColor);
			DrawString(kMonthNames[month - 1], BPoint(bounds.left + 10,
				row.top + (row.Height() + fh.ascent) / 2 - 1));

			for (int day = 1; day <= 31; day++) {
				BRect cell = _CellRect(month, day);
				if (day > DaysInMonth(fYear, month)) {
					SetHighColor(invalid);
					FillRect(cell);
					continue;
				}
				SimpleDate date(fYear, month, day);
				if (DayOfWeek(date) >= 5) {
					SetHighColor(weekend);
					FillRect(cell);
				}
				SetHighColor(kRuleColor);
				StrokeRect(cell);
				if (date == today) {
					SetHighColor(200, 40, 40);
					StrokeRect(cell.InsetByCopy(1, 1));
				}
			}
		}

		_DrawEvents(small);
		_DrawDragRange();
		_DrawLegend(small);
	}

	virtual void MouseDown(BPoint where)
	{
		SimpleDate date;
		if (!_DateAt(where, date))
			return;

		int32 clicks = 1;
		if (Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("clicks", &clicks);

		int64 id = _EventAt(date);
		if (id >= 0) {
			fSelectedId = id;
			Invalidate();
			BMessenger(fOwner).SendMessage(MSG_LIST_SELECTION);
			if (clicks >= 2)
				BMessenger(fOwner).SendMessage(MSG_EDIT_ENTRY);
			return;
		}

		if (fSelectedId >= 0) {
			fSelectedId = -1;
			BMessenger(fOwner).SendMessage(MSG_LIST_SELECTION);
		}

		if (clicks >= 2) {
			_RequestNew(date, date);
			return;
		}

		// start dragging out a date range
		fDragging = true;
		fDragStart = fDragEnd = date;
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		Invalidate();
	}

	virtual void MouseMoved(BPoint where, uint32 transit,
		const BMessage* dragMessage)
	{
		if (!fDragging)
			return;
		SimpleDate date;
		if (_DateAt(where, date) && date != fDragEnd) {
			fDragEnd = date;
			Invalidate();
		}
	}

	virtual void MouseUp(BPoint where)
	{
		if (!fDragging)
			return;
		fDragging = false;
		Invalidate();
		if (fDragStart != fDragEnd)
			_RequestNew(fDragStart, fDragEnd);
	}

private:
	BRect _CellRect(int month, int day) const
	{
		BRect bounds = Bounds();
		float width = (bounds.Width() - kMonthColumn - 10) / 31;
		float height = (bounds.Height() - kTopRow - kLegendHeight - 6) / 12;
		float left = floorf(bounds.left + kMonthColumn + (day - 1) * width);
		float top = floorf(bounds.top + kTopRow + (month - 1) * height);
		return BRect(left, top,
			floorf(bounds.left + kMonthColumn + day * width),
			floorf(bounds.top + kTopRow + month * height));
	}

	bool _DateAt(BPoint where, SimpleDate& date) const
	{
		for (int month = 1; month <= 12; month++) {
			BRect first = _CellRect(month, 1);
			if (where.y < first.top || where.y > first.bottom)
				continue;
			for (int day = 1; day <= DaysInMonth(fYear, month); day++) {
				if (_CellRect(month, day).Contains(where)) {
					date = SimpleDate(fYear, month, day);
					return true;
				}
			}
		}
		return false;
	}

	int64 _EventAt(const SimpleDate& date)
	{
		long day = DayNumber(date);
		int64 found = -1;
		RecordList& events = fStore->Records(kSectionKeys[SECTION_PLANNER]);
		for (size_t i = 0; i < events.size(); i++) {
			long start, end;
			if (EventRange(events[i], start, end) && day >= start
				&& day <= end) {
				found = GetId(events[i]);	// last drawn wins
			}
		}
		return found;
	}

	void _RequestNew(const SimpleDate& a, const SimpleDate& b)
	{
		SimpleDate first = a, last = b;
		if (DayNumber(last) < DayNumber(first))
			std::swap(first, last);
		BMessage message(MSG_NEW_ENTRY);
		message.AddString("start", FormatDate(first).String());
		message.AddString("end", FormatDate(last).String());
		BMessenger(fOwner).SendMessage(&message);
	}

	void _FillRange(long start, long end, rgb_color color, int64 id,
		const BString& title, BFont& font)
	{
		for (int month = 1; month <= 12; month++) {
			long monthStart = DayNumber(SimpleDate(fYear, month, 1));
			long monthEnd = DayNumber(SimpleDate(fYear, month,
				DaysInMonth(fYear, month)));
			long first = std::max(start, monthStart);
			long last = std::min(end, monthEnd);
			if (first > last)
				continue;

			BRect bar = _CellRect(month, first - monthStart + 1)
				| _CellRect(month, last - monthStart + 1);
			bar.InsetBy(1, 3);
			SetHighColor(color);
			FillRoundRect(bar, 3, 3);

			if (id >= 0 && id == fSelectedId) {
				SetHighColor(kInkColor);
				SetPenSize(2);
				StrokeRoundRect(bar, 3, 3);
				SetPenSize(1);
			}

			if (!title.IsEmpty() && bar.Width() > 24) {
				BString text = title;
				font.TruncateString(&text, B_TRUNCATE_END, bar.Width() - 6);
				font_height fh;
				font.GetHeight(&fh);
				SetLowColor(color);
				SetHighColor(kInkColor);
				DrawString(text.String(), BPoint(bar.left + 3,
					bar.top + (bar.Height() + fh.ascent) / 2 - 1));
			}
		}
	}

	void _DrawEvents(BFont& font)
	{
		SetFont(&font);
		RecordList& events = fStore->Records(kSectionKeys[SECTION_PLANNER]);
		for (size_t i = 0; i < events.size(); i++) {
			long start, end;
			if (!EventRange(events[i], start, end))
				continue;
			_FillRange(start, end,
				CategoryColor(GetString(events[i], "category")),
				GetId(events[i]), GetString(events[i], "title"), font);
		}
	}

	void _DrawDragRange()
	{
		if (!fDragging)
			return;
		long start = DayNumber(fDragStart);
		long end = DayNumber(fDragEnd);
		if (end < start)
			std::swap(start, end);
		SetDrawingMode(B_OP_ALPHA);
		for (long day = start; day <= end; day++) {
			SimpleDate date = FromDayNumber(day);
			SetHighColor(255, 200, 40, 140);
			FillRect(_CellRect(date.month, date.day));
		}
		SetDrawingMode(B_OP_COPY);
	}

	void _DrawLegend(BFont& font)
	{
		SetFont(&font);
		BRect bounds = Bounds();
		float x = bounds.left + kMonthColumn;
		float y = bounds.bottom - 10;
		for (int i = 0; i < kPlannerCategoryCount; i++) {
			SetHighColor(kPlannerColors[i]);
			FillRoundRect(BRect(x, y - 10, x + 18, y), 2, 2);
			SetLowColor(kPaperColor);
			SetHighColor(kInkColor);
			DrawString(kPlannerCategories[i], BPoint(x + 24, y - 1));
			x += 34 + font.StringWidth(kPlannerCategories[i]);
		}
		SetHighColor(120, 120, 120);
		DrawString("Drag across days to add an event",
			BPoint(x + 20, y - 1));
	}

	DataStore*	fStore;
	BHandler*	fOwner;
	int			fYear;
	int64		fSelectedId;
	bool		fDragging;
	SimpleDate	fDragStart;
	SimpleDate	fDragEnd;
};


// #pragma mark - PlannerSection


PlannerSection::PlannerSection(int32 index, DataStore* store)
	:
	Section(kSectionNames[index], index, store)
{
	fYear = new YearView(store, this);
	fYearLabel = new BStringView("yearLabel", "");
	BFont font(be_bold_font);
	font.SetSize(16);
	fYearLabel->SetFont(&font);
	fStatus = new BStringView("status", "");

	BGroupView* toolbar = new BGroupView(B_HORIZONTAL, 6);
	toolbar->SetViewColor(kPaperColor);
	BGroupLayout* layout = toolbar->GroupLayout();
	layout->SetInsets(10, 6, 10, 6);
	layout->AddView(MakeButton("\xE2\x97\x80", MSG_PREV));
	layout->AddView(fYearLabel);
	layout->AddView(MakeButton("\xE2\x96\xB6", MSG_NEXT));
	layout->AddView(MakeButton("This year", MSG_GO_TODAY));
	layout->AddItem(BSpaceLayoutItem::CreateHorizontalStrut(12));
	layout->AddView(fStatus);
	layout->AddItem(BSpaceLayoutItem::CreateGlue());
	layout->AddView(MakeButton("New event", MSG_NEW_ENTRY));
	layout->AddView(MakeButton("Edit", MSG_EDIT_ENTRY));
	layout->AddView(MakeButton("Delete", MSG_DELETE_ENTRY));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(new SectionHeader(index))
		.Add(toolbar)
		.Add(fYear);

	_UpdateLabels();
}


void
PlannerSection::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_PREV:
			fYear->SetYear(fYear->Year() - 1);
			_UpdateLabels();
			break;
		case MSG_NEXT:
			fYear->SetYear(fYear->Year() + 1);
			_UpdateLabels();
			break;
		case MSG_GO_TODAY:
			fYear->SetYear(Today().year);
			_UpdateLabels();
			break;
		case MSG_LIST_SELECTION:
			_UpdateLabels();
			break;

		case MSG_NEW_ENTRY:
		{
			const char* startText;
			const char* endText;
			SimpleDate start, end;
			if (message->FindString("start", &startText) == B_OK
				&& message->FindString("end", &endText) == B_OK
				&& ParseDate(startText, start) && ParseDate(endText, end)) {
				_NewEvent(start, end);
			} else
				NewEntry();
			break;
		}

		default:
			Section::MessageReceived(message);
			break;
	}
}


void
PlannerSection::Reload()
{
	_UpdateLabels();
	fYear->Invalidate();
}


void
PlannerSection::NewEntry()
{
	SimpleDate today = Today();
	SimpleDate start = fYear->Year() == today.year
		? today : SimpleDate(fYear->Year(), 1, 1);
	_NewEvent(start, start);
}


void
PlannerSection::_NewEvent(const SimpleDate& start, const SimpleDate& end)
{
	BMessage record;
	SetString(record, "start", FormatDate(start).String());
	SetString(record, "end", FormatDate(end).String());
	SetString(record, "category", start == end ? "Meeting" : "Vacation");
	OpenEditor("New planner event", kPlannerFields, kPlannerFieldCount,
		record);
}


void
PlannerSection::EditSelected()
{
	const BMessage* record = fStore->Find(Key(), fYear->SelectedId());
	if (record == NULL)
		return;
	OpenEditor("Edit planner event", kPlannerFields, kPlannerFieldCount,
		*record);
}


void
PlannerSection::DeleteSelected()
{
	const BMessage* record = fStore->Find(Key(), fYear->SelectedId());
	if (record == NULL)
		return;
	int64 id = GetId(*record);
	if (!ConfirmDelete(GetString(*record, "title").String()))
		return;
	fStore->Remove(Key(), id);
	fYear->ClearSelection();
	DataChanged();
}


void
PlannerSection::_UpdateLabels()
{
	BString year;
	year << fYear->Year();
	fYearLabel->SetText(year.String());

	BString status;
	const BMessage* record = fStore->Find(Key(), fYear->SelectedId());
	if (record != NULL) {
		SimpleDate start, end;
		ParseDate(GetString(*record, "start").String(), start);
		if (!ParseDate(GetString(*record, "end").String(), end))
			end = start;
		status << GetString(*record, "title") << "  \xE2\x80\x94  "
			<< FormatShort(start);
		if (end != start)
			status << " \xE2\x80\x93 " << FormatShort(end);
	}
	fStatus->SetText(status.String());
}
