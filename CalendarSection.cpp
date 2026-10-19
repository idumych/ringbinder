/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "CalendarSection.h"

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
#include <vector>


static const FieldSpec kAppointmentFields[] = {
	{ "title",		"Description",	FIELD_TEXT,		NULL, true },
	{ "date",		"Date",			FIELD_DATE,		NULL, true },
	{ "start",		"Start time",	FIELD_TIME,		NULL, false },
	{ "end",		"End time",		FIELD_TIME,		NULL, false },
	{ "location",	"Location",		FIELD_TEXT,		NULL, false },
	{ "category",	"Category",		FIELD_CHOICE,
		"Business|Personal|Travel|Holiday|Other", false },
	{ "notes",		"Notes",		FIELD_MULTILINE, NULL, false }
};
static const int32 kAppointmentFieldCount
	= sizeof(kAppointmentFields) / sizeof(kAppointmentFields[0]);

enum {
	ENTRY_APPOINTMENT,
	ENTRY_ANNIVERSARY,
	ENTRY_TODO
};

struct DayEntry {
	BString	sortKey;
	BString	time;
	BString	text;
	int64	id;
	int		kind;
};

static bool
DayEntryLess(const DayEntry& a, const DayEntry& b)
{
	if (a.kind != b.kind)
		return a.kind < b.kind;
	return a.sortKey.Compare(b.sortKey) < 0;
}


// #pragma mark - WeekView


class WeekView : public BView {
public:
	WeekView(DataStore* store, BHandler* owner)
		:
		BView("week", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fStore(store),
		fOwner(owner),
		fMonday(StartOfWeek(Today())),
		fSelectedId(-1),
		fSelectedDate(Today())
	{
		SetViewColor(B_TRANSPARENT_COLOR);
		SetExplicitMinSize(BSize(560, 380));
	}

	SimpleDate Monday() const { return fMonday; }
	int64 SelectedId() const { return fSelectedId; }
	SimpleDate SelectedDate() const { return fSelectedDate; }

	void SetMonday(const SimpleDate& monday)
	{
		fMonday = monday;
		// keep the selected weekday when paging through weeks
		fSelectedDate = AddDays(monday, DayOfWeek(fSelectedDate));
		fSelectedId = -1;
		Invalidate();
	}

	void ClearSelectedEntry()
	{
		fSelectedId = -1;
		Invalidate();
	}

	virtual void Draw(BRect updateRect)
	{
		fEntryHits.clear();
		fDayHits.clear();

		BRect bounds = Bounds();
		SetHighColor(kBinderColor);
		FillRect(bounds);

		float gutter = 36;
		float middle = floorf(bounds.left + bounds.Width() / 2);
		BRect left(bounds.left + 10, bounds.top + 10,
			middle - gutter / 2, bounds.bottom - 10);
		BRect right(middle + gutter / 2, bounds.top + 10,
			bounds.right - 10, bounds.bottom - 10);

		_DrawPage(left);
		_DrawPage(right);
		_DrawRings(bounds, middle);

		// Left page: Monday - Wednesday
		float height = left.Height() / 3;
		for (int i = 0; i < 3; i++) {
			BRect box(left.left, left.top + i * height, left.right,
				left.top + (i + 1) * height);
			_DrawDay(box.InsetByCopy(10, 5), AddDays(fMonday, i));
		}

		// Right page: Thursday, Friday, then a half-height weekend
		height = right.Height() / 3;
		float tops[5] = { 0, 1, 2, 2.5f, 3 };
		for (int i = 0; i < 4; i++) {
			BRect box(right.left, right.top + tops[i] * height, right.right,
				right.top + tops[i + 1] * height);
			_DrawDay(box.InsetByCopy(10, 5), AddDays(fMonday, 3 + i));
		}
	}

	virtual void MouseDown(BPoint where)
	{
		int32 clicks = 1;
		if (Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("clicks", &clicks);

		for (size_t i = 0; i < fEntryHits.size(); i++) {
			if (!fEntryHits[i].rect.Contains(where))
				continue;
			fSelectedId = fEntryHits[i].id;
			fSelectedDate = fEntryHits[i].date;
			Invalidate();
			if (clicks >= 2)
				BMessenger(fOwner).SendMessage(MSG_EDIT_ENTRY);
			return;
		}

		for (size_t i = 0; i < fDayHits.size(); i++) {
			if (!fDayHits[i].rect.Contains(where))
				continue;
			fSelectedId = -1;
			fSelectedDate = fDayHits[i].date;
			Invalidate();
			if (clicks >= 2) {
				BMessage message(MSG_NEW_ENTRY);
				message.AddString("date", FormatDate(fSelectedDate).String());
				BMessenger(fOwner).SendMessage(&message);
			}
			return;
		}
	}

private:
	struct Hit {
		BRect		rect;
		int64		id;
		SimpleDate	date;
	};

	void _DrawPage(BRect page)
	{
		SetHighColor(tint_color(kBinderColor, B_DARKEN_2_TINT));
		FillRect(page.OffsetByCopy(3, 3));
		SetHighColor(kPaperColor);
		FillRect(page);
	}

	void _DrawRings(BRect bounds, float middle)
	{
		int rings = 6;
		for (int i = 0; i < rings; i++) {
			float y = floorf(bounds.top + (i + 0.5f) * bounds.Height() / rings);
			BRect ring(middle - 26, y - 5, middle + 26, y + 5);
			SetHighColor(120, 120, 128);
			FillRoundRect(ring.OffsetByCopy(0, 2), 5, 5);
			SetHighColor(205, 205, 215);
			FillRoundRect(ring, 5, 5);
			SetHighColor(250, 250, 255);
			StrokeLine(BPoint(ring.left + 5, ring.top + 2),
				BPoint(ring.right - 5, ring.top + 2));
		}
	}

	void _CollectEntries(const SimpleDate& date, std::vector<DayEntry>& out)
	{
		BString iso = FormatDate(date);

		RecordList& appointments
			= fStore->Records(kSectionKeys[SECTION_CALENDAR]);
		for (size_t i = 0; i < appointments.size(); i++) {
			const BMessage& record = appointments[i];
			if (GetString(record, "date") != iso)
				continue;
			DayEntry entry;
			entry.kind = ENTRY_APPOINTMENT;
			entry.id = GetId(record);
			BString start = GetString(record, "start");
			BString end = GetString(record, "end");
			entry.sortKey = start;
			entry.time = start;
			if (!end.IsEmpty())
				entry.time << "\xE2\x80\x93" << end;
			entry.text = GetString(record, "title");
			BString location = GetString(record, "location");
			if (!location.IsEmpty())
				entry.text << " (" << location << ")";
			out.push_back(entry);
		}

		RecordList& anniversaries
			= fStore->Records(kSectionKeys[SECTION_ANNIVERSARY]);
		for (size_t i = 0; i < anniversaries.size(); i++) {
			const BMessage& record = anniversaries[i];
			SimpleDate when;
			if (!ParseDate(GetString(record, "date").String(), when)
				|| when.month != date.month || when.day != date.day) {
				continue;
			}
			DayEntry entry;
			entry.kind = ENTRY_ANNIVERSARY;
			entry.id = -1;
			entry.text = "\xE2\x98\x85 ";
			entry.text << GetString(record, "title");
			int years = date.year - when.year;
			if (years > 0) {
				BString count;
				count.SetToFormat(" (%d)", years);
				entry.text << count;
			}
			out.push_back(entry);
		}

		RecordList& todos = fStore->Records(kSectionKeys[SECTION_TODO]);
		for (size_t i = 0; i < todos.size(); i++) {
			const BMessage& record = todos[i];
			if (GetBool(record, "done") || GetString(record, "due") != iso)
				continue;
			DayEntry entry;
			entry.kind = ENTRY_TODO;
			entry.id = -1;
			entry.sortKey = GetString(record, "priority");
			entry.text = "\xE2\x98\x90 ";
			entry.text << GetString(record, "title");
			out.push_back(entry);
		}

		std::stable_sort(out.begin(), out.end(), DayEntryLess);
	}

	void _DrawDay(BRect box, const SimpleDate& date)
	{
		bool isToday = date == Today();
		bool isSelected = date == fSelectedDate;

		Hit dayHit = { box, -1, date };
		fDayHits.push_back(dayHit);

		BFont bold(be_bold_font);
		bold.SetSize(13);
		font_height boldHeight;
		bold.GetHeight(&boldHeight);
		float headerHeight = ceilf(boldHeight.ascent + boldHeight.descent + 8);

		BRect header(box.left, box.top, box.right, box.top + headerHeight);
		BRect body(box.left, header.bottom + 1, box.right, box.bottom);

		rgb_color headerColor = isToday
			? kSectionColors[SECTION_CALENDAR] : kPaperShade;
		SetHighColor(headerColor);
		FillRect(header);
		if (isSelected) {
			SetHighColor(255, 247, 205);
			FillRect(body);
		}

		SetFont(&bold);
		SetLowColor(headerColor);
		SetHighColor(kInkColor);
		BString title;
		title.SetToFormat("%s %d %s", kDayNames[DayOfWeek(date)], date.day,
			kMonthNames[date.month - 1]);
		DrawString(title.String(), BPoint(header.left + 6,
			header.top + 4 + boldHeight.ascent));

		BFont plain(be_plain_font);
		plain.SetSize(12);
		SetFont(&plain);
		font_height fh;
		plain.GetHeight(&fh);
		float lineHeight = ceilf(fh.ascent + fh.descent + fh.leading + 5);

		SetHighColor(kRuleColor);
		for (float y = body.top + lineHeight; y < body.bottom; y += lineHeight)
			StrokeLine(BPoint(body.left, y), BPoint(body.right, y));
		SetHighColor(tint_color(kPaperShade, B_DARKEN_2_TINT));
		StrokeRect(box);

		std::vector<DayEntry> entries;
		_CollectEntries(date, entries);

		int maxLines = (int)floorf(body.Height() / lineHeight);
		float timeWidth = plain.StringWidth("00:00\xE2\x80\x93" "00:00  ");

		for (size_t i = 0; i < entries.size(); i++) {
			const DayEntry& entry = entries[i];
			BRect line(body.left + 1, body.top + i * lineHeight,
				body.right - 1, body.top + (i + 1) * lineHeight - 1);
			float baseline = line.bottom - fh.descent - 2;

			if ((int)i == maxLines - 1 && entries.size() > (size_t)maxLines) {
				BString more;
				more.SetToFormat("+ %d more\xE2\x80\xA6",
					(int)(entries.size() - i));
				SetHighColor(120, 120, 120);
				SetLowColor(isSelected ? make_color(255, 247, 205)
					: kPaperColor);
				DrawString(more.String(), BPoint(line.left + 6, baseline));
				break;
			}
			if ((int)i >= maxLines)
				break;

			rgb_color background = isSelected
				? make_color(255, 247, 205) : kPaperColor;
			rgb_color ink = kInkColor;
			if (entry.kind == ENTRY_APPOINTMENT && entry.id == fSelectedId) {
				background = ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
				ink = ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR);
				SetHighColor(background);
				FillRect(line);
			} else if (entry.kind == ENTRY_ANNIVERSARY) {
				ink = make_color(170, 40, 40);
			} else if (entry.kind == ENTRY_TODO) {
				ink = make_color(170, 90, 20);
			}

			SetLowColor(background);
			SetHighColor(ink);
			float x = line.left + 6;
			if (entry.kind == ENTRY_APPOINTMENT) {
				DrawString(entry.time.String(), BPoint(x, baseline));
				x += timeWidth;
			}
			BString text = entry.text;
			plain.TruncateString(&text, B_TRUNCATE_END, line.right - x - 4);
			DrawString(text.String(), BPoint(x, baseline));

			if (entry.kind == ENTRY_APPOINTMENT) {
				Hit hit = { line, entry.id, date };
				fEntryHits.push_back(hit);
			}
		}
	}

	DataStore*			fStore;
	BHandler*			fOwner;
	SimpleDate			fMonday;
	int64				fSelectedId;
	SimpleDate			fSelectedDate;
	std::vector<Hit>	fEntryHits;
	std::vector<Hit>	fDayHits;
};


// #pragma mark - CalendarSection


CalendarSection::CalendarSection(int32 index, DataStore* store)
	:
	Section(kSectionNames[index], index, store)
{
	fWeek = new WeekView(store, this);
	fLabel = new BStringView("week", "");
	BFont font(be_bold_font);
	font.SetSize(14);
	fLabel->SetFont(&font);

	BGroupView* toolbar = new BGroupView(B_HORIZONTAL, 6);
	toolbar->SetViewColor(kPaperColor);
	BGroupLayout* layout = toolbar->GroupLayout();
	layout->SetInsets(10, 6, 10, 6);
	layout->AddView(MakeButton("\xE2\x97\x80 Previous week", MSG_PREV));
	layout->AddView(MakeButton("This week", MSG_GO_TODAY));
	layout->AddView(MakeButton("Next week \xE2\x96\xB6", MSG_NEXT));
	layout->AddItem(BSpaceLayoutItem::CreateHorizontalStrut(12));
	layout->AddView(fLabel);
	layout->AddItem(BSpaceLayoutItem::CreateGlue());
	layout->AddView(MakeButton("New appointment", MSG_NEW_ENTRY));
	layout->AddView(MakeButton("Edit", MSG_EDIT_ENTRY));
	layout->AddView(MakeButton("Delete", MSG_DELETE_ENTRY));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(new SectionHeader(index))
		.Add(toolbar)
		.Add(fWeek);

	_UpdateLabel();
}


void
CalendarSection::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_PREV:
			fWeek->SetMonday(AddDays(fWeek->Monday(), -7));
			_UpdateLabel();
			break;
		case MSG_NEXT:
			fWeek->SetMonday(AddDays(fWeek->Monday(), 7));
			_UpdateLabel();
			break;
		case MSG_GO_TODAY:
			fWeek->SetMonday(StartOfWeek(Today()));
			_UpdateLabel();
			break;

		case MSG_NEW_ENTRY:
		{
			const char* text;
			SimpleDate date;
			if (message->FindString("date", &text) == B_OK
				&& ParseDate(text, date)) {
				_NewEntryOn(date);
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
CalendarSection::Reload()
{
	_UpdateLabel();
	fWeek->Invalidate();
}


void
CalendarSection::NewEntry()
{
	_NewEntryOn(fWeek->SelectedDate());
}


void
CalendarSection::_NewEntryOn(const SimpleDate& date)
{
	BMessage record;
	SetString(record, "date", FormatDate(date).String());
	SetString(record, "start", "09:00");
	SetString(record, "end", "10:00");
	OpenEditor("New appointment", kAppointmentFields,
		kAppointmentFieldCount, record);
}


void
CalendarSection::EditSelected()
{
	const BMessage* record = fStore->Find(Key(), fWeek->SelectedId());
	if (record == NULL)
		return;
	OpenEditor("Edit appointment", kAppointmentFields,
		kAppointmentFieldCount, *record);
}


void
CalendarSection::DeleteSelected()
{
	const BMessage* record = fStore->Find(Key(), fWeek->SelectedId());
	if (record == NULL)
		return;
	int64 id = GetId(*record);
	if (!ConfirmDelete(GetString(*record, "title").String()))
		return;
	fStore->Remove(Key(), id);
	fWeek->ClearSelectedEntry();
	DataChanged();
}


void
CalendarSection::_UpdateLabel()
{
	SimpleDate first = fWeek->Monday();
	SimpleDate last = AddDays(first, 6);
	BString label;
	if (first.month == last.month) {
		label.SetToFormat("%d \xE2\x80\x93 %d %s %d", first.day, last.day,
			kMonthNames[last.month - 1], last.year);
	} else if (first.year == last.year) {
		label.SetToFormat("%d %s \xE2\x80\x93 %d %s %d", first.day,
			kMonthNames[first.month - 1], last.day,
			kMonthNames[last.month - 1], last.year);
	} else {
		label.SetToFormat("%d %s %d \xE2\x80\x93 %d %s %d", first.day,
			kMonthNames[first.month - 1], first.year, last.day,
			kMonthNames[last.month - 1], last.year);
	}
	fLabel->SetText(label.String());
}
