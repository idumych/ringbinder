/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "AnniversarySection.h"

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

#include "Spread.h"


static const FieldSpec kAnniversaryFields[] = {
	{ "title",		"Occasion",		FIELD_TEXT,		NULL, true },
	{ "date",		"Date",			FIELD_DATE,		NULL, true },
	{ "category",	"Category",		FIELD_CHOICE,
		"Birthday|Anniversary|Holiday|Other", false },
	{ "notes",		"Notes",		FIELD_MULTILINE, NULL, false }
};
static const int32 kAnniversaryFieldCount
	= sizeof(kAnniversaryFields) / sizeof(kAnniversaryFields[0]);

static const int kMonthsPerPage = 3;
static const rgb_color kMonthBandColor = { 238, 232, 212, 255 };
static const rgb_color kSoonColor = { 150, 146, 130, 255 };
static const rgb_color kTodayColor = { 190, 40, 40, 255 };


// #pragma mark - MonthPage


// One page of the spread: three month boxes, each listing that month's
// occasions by day.
class MonthPage : public BView {
public:
	MonthPage(AnniversarySection* owner, DataStore* store, bool rightPage)
		:
		BView(rightPage ? "rightMonths" : "leftMonths",
			B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fOwner(owner),
		fStore(store),
		fRightPage(rightPage)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
		SetExplicitMinSize(BSize(220, 300));
	}

	virtual void Draw(BRect updateRect)
	{
		fHits.clear();
		fMonthHits.clear();

		BRect bounds = Bounds();
		SetHighColor(kPaperColor);
		FillRect(bounds);

		BFont bold(be_bold_font);
		bold.SetSize(13);
		BFont plain(be_plain_font);
		plain.SetSize(12);
		BFont small(be_plain_font);
		small.SetSize(10);
		font_height boldHeight, plainHeight;
		bold.GetHeight(&boldHeight);
		plain.GetHeight(&plainHeight);
		float lineHeight = ceilf(plainHeight.ascent + plainHeight.descent) + 6;

		SimpleDate today = Today();
		float boxHeight = floorf(bounds.Height() / kMonthsPerPage);
		for (int i = 0; i < kMonthsPerPage; i++) {
			int month = _FirstMonth() + i;
			BRect box(bounds.left + 10, bounds.top + i * boxHeight + 6,
				bounds.right - 10, bounds.top + (i + 1) * boxHeight - 4);
			fMonthHits.push_back(box);

			// month heading, on the outer side of the page as on paper
			BRect band(box.left, box.top, box.right,
				box.top + boldHeight.ascent + 10);
			SetHighColor(kMonthBandColor);
			FillRect(band);
			SetHighColor(kSectionColors[SECTION_ANNIVERSARY]);
			FillRect(BRect(band.left, band.bottom - 1, band.right,
				band.bottom));
			SetFont(&bold);
			SetLowColor(kMonthBandColor);
			SetHighColor(kInkColor);
			BString heading(kMonthNames[month - 1]);
			float width = bold.StringWidth(heading.String());
			DrawString(heading.String(), BPoint(fRightPage
				? band.right - 6 - width : band.left + 6,
				band.bottom - 6));

			// ruled lines
			SetHighColor(kRuleColor);
			for (float y = band.bottom + lineHeight; y < box.bottom;
					y += lineHeight) {
				StrokeLine(BPoint(box.left, y), BPoint(box.right, y));
			}

			std::vector<const BMessage*> occasions;
			_Collect(month, occasions);
			float y = band.bottom;
			for (size_t j = 0; j < occasions.size(); j++) {
				if (y + lineHeight > box.bottom) {
					SetFont(&small);
					SetLowColor(kPaperColor);
					SetHighColor(kSoonColor);
					BString more;
					more.SetToFormat("+ %d more", (int)(occasions.size() - j));
					DrawString(more.String(), BPoint(box.right - 6
						- small.StringWidth(more.String()), box.bottom - 2));
					break;
				}
				BRect line(box.left, y + 1, box.right, y + lineHeight - 1);
				_DrawOccasion(*occasions[j], line, plain, small, month,
					today);
				y += lineHeight;
			}
		}
	}

	virtual void MouseDown(BPoint where)
	{
		int32 clicks = 1;
		if (Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("clicks", &clicks);

		for (size_t i = 0; i < fHits.size(); i++) {
			if (!fHits[i].rect.Contains(where))
				continue;
			fOwner->Select(fHits[i].id);
			if (clicks >= 2)
				BMessenger(fOwner).SendMessage(MSG_EDIT_ENTRY);
			return;
		}

		for (size_t i = 0; i < fMonthHits.size(); i++) {
			if (!fMonthHits[i].Contains(where))
				continue;
			fOwner->Select(-1);
			if (clicks >= 2) {
				int month = _FirstMonth() + i;
				SimpleDate today = Today();
				int day = today.month == month ? today.day : 1;
				BMessage message(MSG_NEW_ENTRY);
				message.AddString("date", FormatDate(
					SimpleDate(fOwner->Year(), month, day)).String());
				BMessenger(fOwner).SendMessage(&message);
			}
			return;
		}
	}

private:
	struct Hit {
		BRect	rect;
		int64	id;
	};

	int _FirstMonth() const
	{
		return fOwner->FirstMonth() + (fRightPage ? kMonthsPerPage : 0);
	}

	void _Collect(int month, std::vector<const BMessage*>& out)
	{
		RecordList& records
			= fStore->Records(kSectionKeys[SECTION_ANNIVERSARY]);
		for (size_t i = 0; i < records.size(); i++) {
			SimpleDate date;
			if (ParseDate(GetString(records[i], "date").String(), date)
				&& date.month == month) {
				out.push_back(&records[i]);
			}
		}
		std::sort(out.begin(), out.end(), _DayLess);
	}

	static bool _DayLess(const BMessage* a, const BMessage* b)
	{
		SimpleDate dateA, dateB;
		ParseDate(GetString(*a, "date").String(), dateA);
		ParseDate(GetString(*b, "date").String(), dateB);
		if (dateA.day != dateB.day)
			return dateA.day < dateB.day;
		return GetString(*a, "title").ICompare(GetString(*b, "title")) < 0;
	}

	void _DrawOccasion(const BMessage& record, BRect line, BFont& plain,
		BFont& small, int month, const SimpleDate& today)
	{
		SimpleDate date;
		ParseDate(GetString(record, "date").String(), date);
		int64 id = GetId(record);
		bool selected = id == fOwner->SelectedId();
		bool isToday = today.year == fOwner->Year()
			&& today.month == month && today.day == date.day;

		Hit hit = { line, id };
		fHits.push_back(hit);

		rgb_color background = selected
			? ui_color(B_LIST_SELECTED_BACKGROUND_COLOR) : kPaperColor;
		rgb_color ink = selected
			? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR)
			: (isToday ? kTodayColor : kInkColor);
		if (selected) {
			SetHighColor(background);
			FillRect(line);
		}

		font_height fh;
		plain.GetHeight(&fh);
		float baseline = floorf(line.top
			+ (line.Height() + fh.ascent - fh.descent) / 2);

		SetLowColor(background);
		SetHighColor(ink);
		SetFont(&plain);
		BString day;
		day << date.day;
		DrawString(day.String(), BPoint(line.left + 26
			- plain.StringWidth(day.String()), baseline));

		BString text("\xE2\x98\x85 ");
		text << GetString(record, "title");
		int years = fOwner->Year() - date.year;
		if (years > 0 && date.year > 1800) {
			BString count;
			count.SetToFormat(" (%d)", years);
			text << count;
		}

		// how soon, for the next few weeks
		BString soon;
		int until = DaysUntilNext(date);
		SimpleDate next = AddDays(today, until);
		if (next.year == fOwner->Year() && until <= 30) {
			if (until == 0)
				soon = "today";
			else if (until == 1)
				soon = "tomorrow";
			else
				soon.SetToFormat("in %d days", until);
		}
		float soonWidth = soon.IsEmpty()
			? 0 : small.StringWidth(soon.String()) + 10;

		plain.TruncateString(&text, B_TRUNCATE_END,
			line.right - line.left - 40 - soonWidth);
		DrawString(text.String(), BPoint(line.left + 36, baseline));

		if (!soon.IsEmpty()) {
			SetFont(&small);
			SetHighColor(selected ? ink : kSoonColor);
			DrawString(soon.String(), BPoint(line.right - 6
				- small.StringWidth(soon.String()), baseline));
		}
	}

	AnniversarySection*	fOwner;
	DataStore*			fStore;
	bool				fRightPage;
	std::vector<Hit>	fHits;
	std::vector<BRect>	fMonthHits;
};


// #pragma mark - AnniversarySection


AnniversarySection::AnniversarySection(int32 index, DataStore* store)
	:
	Section(kSectionNames[index], index, store),
	fYear(Today().year),
	fFirstMonth(Today().month <= 6 ? 1 : 7),
	fSelectedId(-1)
{
	fLabel = new BStringView("halfLabel", "");
	BFont font(be_bold_font);
	font.SetSize(14);
	fLabel->SetFont(&font);

	BGroupView* toolbar = new BGroupView(B_HORIZONTAL, 6);
	toolbar->SetViewColor(kPaperColor);
	BGroupLayout* layout = toolbar->GroupLayout();
	layout->SetInsets(10, 6, 10, 6);
	layout->AddView(MakeButton("\xE2\x97\x80", MSG_PREV));
	layout->AddView(MakeButton("\xE2\x96\xB6", MSG_NEXT));
	layout->AddView(MakeButton("Today", MSG_GO_TODAY));
	layout->AddItem(BSpaceLayoutItem::CreateHorizontalStrut(12));
	layout->AddView(fLabel);
	layout->AddItem(BSpaceLayoutItem::CreateGlue());
	layout->AddView(MakeButton("New Occasion", MSG_NEW_ENTRY));
	layout->AddView(MakeButton("Edit", MSG_EDIT_ENTRY));
	layout->AddView(MakeButton("Delete", MSG_DELETE_ENTRY));

	fLeft = new MonthPage(this, store, false);
	fRight = new MonthPage(this, store, true);
	SpreadView* spread = new SpreadView();
	spread->LeftPage()->GroupLayout()->AddView(fLeft);
	spread->RightPage()->GroupLayout()->AddView(fRight);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(new SectionHeader(index))
		.Add(toolbar)
		.Add(spread);

	_Refresh();
}


void
AnniversarySection::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_PREV:
			if (fFirstMonth == 7)
				_ShowHalf(fYear, 1);
			else
				_ShowHalf(fYear - 1, 7);
			break;
		case MSG_NEXT:
			if (fFirstMonth == 1)
				_ShowHalf(fYear, 7);
			else
				_ShowHalf(fYear + 1, 1);
			break;
		case MSG_GO_TODAY:
			_ShowHalf(Today().year, Today().month <= 6 ? 1 : 7);
			break;

		case MSG_NEW_ENTRY:
		{
			const char* text;
			SimpleDate date;
			if (message->FindString("date", &text) == B_OK
				&& ParseDate(text, date)) {
				_NewOn(date);
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
AnniversarySection::Reload()
{
	if (fStore->Find(Key(), fSelectedId) == NULL)
		fSelectedId = -1;
	_Refresh();
}


void
AnniversarySection::Select(int64 id)
{
	fSelectedId = id;
	_Refresh();
}


void
AnniversarySection::NewEntry()
{
	SimpleDate today = Today();
	int month = today.month;
	if (fYear != today.year || month < fFirstMonth || month >= fFirstMonth + 6)
		month = fFirstMonth;
	_NewOn(SimpleDate(fYear, month, month == today.month ? today.day : 1));
}


void
AnniversarySection::_NewOn(const SimpleDate& date)
{
	BMessage record;
	SetString(record, "date", FormatDate(date).String());
	SetString(record, "category", "Birthday");
	OpenEditor("New Occasion", kAnniversaryFields, kAnniversaryFieldCount,
		record);
}


void
AnniversarySection::EditSelected()
{
	const BMessage* record = fStore->Find(Key(), fSelectedId);
	if (record == NULL)
		return;
	OpenEditor("Edit Occasion", kAnniversaryFields, kAnniversaryFieldCount,
		*record);
}


void
AnniversarySection::DeleteSelected()
{
	const BMessage* record = fStore->Find(Key(), fSelectedId);
	if (record == NULL)
		return;
	int64 id = GetId(*record);
	if (!ConfirmDelete(GetString(*record, "title").String()))
		return;
	fStore->Remove(Key(), id);
	fSelectedId = -1;
	DataChanged();
}


void
AnniversarySection::_ShowHalf(int year, int firstMonth)
{
	fYear = year;
	fFirstMonth = firstMonth;
	_Refresh();
}


void
AnniversarySection::_Refresh()
{
	BString label;
	label << kMonthNames[fFirstMonth - 1] << " \xE2\x80\x93 "
		<< kMonthNames[fFirstMonth + 4] << " " << fYear;
	fLabel->SetText(label.String());
	fLeft->Invalidate();
	fRight->Invalidate();
}
