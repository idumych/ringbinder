/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "Spread.h"

#include <GroupLayout.h>
#include <LayoutBuilder.h>

#include <math.h>

#include "Constants.h"


static const float kGutter = 36;		// as on the Calendar
static const float kCoverMargin = 10;
static const float kRingOverlap = 10;	// how far the rings reach onto a page
static const int kRingCount = 6;


static BRect
RingRect(BRect bounds, float middle, int index)
{
	float y = floorf(bounds.top + (index + 0.5f) * bounds.Height()
		/ kRingCount);
	return BRect(middle - 26, y - 5, middle + 26, y + 5);
}


static void
DrawRing(BView* view, BRect ring)
{
	view->SetHighColor(120, 120, 128);
	view->FillRoundRect(ring.OffsetByCopy(0, 2), 5, 5);
	view->SetHighColor(205, 205, 215);
	view->FillRoundRect(ring, 5, 5);
	view->SetHighColor(250, 250, 255);
	view->StrokeLine(BPoint(ring.left + 5, ring.top + 2),
		BPoint(ring.right - 5, ring.top + 2));
}


void
DrawBinderRings(BView* view, BRect bounds, float middle)
{
	for (int i = 0; i < kRingCount; i++)
		DrawRing(view, RingRect(bounds, middle, i));
}


// #pragma mark - PageView


// One page of the spread. It draws the ends of the rings that reach over
// its inner edge (the spread draws the part across the gap), and keeps an
// inner margin free for them.
class PageView : public BGroupView {
public:
	PageView(const char* name, bool leftPage)
		:
		BGroupView(name, B_VERTICAL, 0),
		fLeftPage(leftPage)
	{
		SetFlags(Flags() | B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE);
		// Both pages ask for the same width, so the spread splits evenly
		// whatever is on them, as the pages of a real binder do.
		SetExplicitPreferredSize(BSize(100, B_SIZE_UNSET));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
		SetViewColor(kPaperColor);
		SetLowColor(kPaperColor);
		if (leftPage)
			GroupLayout()->SetInsets(0, 0, kRingOverlap, 0);
		else
			GroupLayout()->SetInsets(kRingOverlap, 0, 0, 0);
	}

	virtual void Draw(BRect updateRect)
	{
		BView* spread = Parent();
		if (spread == NULL)
			return;
		BRect spreadBounds = spread->Bounds();
		float middle = fLeftPage
			? Frame().right + kGutter / 2 : Frame().left - kGutter / 2;
		for (int i = 0; i < kRingCount; i++) {
			BRect ring = RingRect(spreadBounds, middle, i);
			// from the spread's coordinates to this page's
			ring.OffsetBy(-Frame().left, -Frame().top);
			DrawRing(this, ring);
		}
	}

private:
	bool	fLeftPage;
};


// #pragma mark - SpreadView


SpreadView::SpreadView()
	:
	BView("spread", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fLeft(new PageView("leftPage", true)),
	fRight(new PageView("rightPage", false))
{
	SetViewColor(B_TRANSPARENT_COLOR);
	BLayoutBuilder::Group<>(this, B_HORIZONTAL, kGutter)
		.SetInsets(kCoverMargin)
		.Add(fLeft, 1)
		.Add(fRight, 1);
}


void
SpreadView::Draw(BRect updateRect)
{
	SetHighColor(kBinderColor);
	FillRect(Bounds());

	// the pages' shadows on the cover
	SetHighColor(tint_color(kBinderColor, B_DARKEN_2_TINT));
	FillRect(fLeft->Frame().OffsetByCopy(3, 3));
	FillRect(fRight->Frame().OffsetByCopy(3, 3));

	// the rings across the gap
	float middle = floorf((fLeft->Frame().right + fRight->Frame().left) / 2);
	DrawBinderRings(this, Bounds(), middle);
}
