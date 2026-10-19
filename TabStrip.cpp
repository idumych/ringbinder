/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "TabStrip.h"

#include <Font.h>
#include <Window.h>

#include <algorithm>
#include <math.h>

#include "Constants.h"


static const float kSpineWidth = 18;
static const float kTabDepth = 30;		// how far a tab sticks out
static const float kTabGap = 4;
static const float kTabMargin = 14;		// above the first and below the last


static rgb_color
Shade(rgb_color color, float factor)
{
	return make_color((uint8)std::min(255.0f, color.red * factor),
		(uint8)std::min(255.0f, color.green * factor),
		(uint8)std::min(255.0f, color.blue * factor));
}


TabStrip::TabStrip(bool leftEdge)
	:
	BView(leftEdge ? "leftTabs" : "rightTabs",
		B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fLeftEdge(leftEdge),
	fSelected(0)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	float width = kTabDepth + 6 + (leftEdge ? kSpineWidth : 0);
	SetExplicitMinSize(BSize(width, SECTION_COUNT * 70));
	SetExplicitMaxSize(BSize(width, B_SIZE_UNLIMITED));
}


void
TabStrip::SetSelected(int32 index)
{
	if (index == fSelected)
		return;
	fSelected = index;
	Invalidate();
}


bool
TabStrip::_IsOnThisEdge(int32 index) const
{
	return fLeftEdge ? index <= fSelected : index > fSelected;
}


BRect
TabStrip::_TabRect(int32 index) const
{
	// Both strips use the same slots, so a tab keeps its height when it
	// moves from one edge to the other.
	BRect bounds = Bounds();
	float height = std::min(120.0f, floorf((bounds.Height() - 2 * kTabMargin)
		/ SECTION_COUNT) - kTabGap);
	float top = floorf(bounds.top + kTabMargin + index * (height + kTabGap));

	if (fLeftEdge) {
		// The open section's tab stands a little proud of the others.
		float outer = bounds.right - kTabDepth - 2
			+ (index == fSelected ? -4 : 0);
		return BRect(outer, top, bounds.right, top + height);
	}
	return BRect(bounds.left, top, bounds.left + kTabDepth, top + height);
}


void
TabStrip::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	SetHighColor(kBinderColor);
	FillRect(bounds);

	if (fLeftEdge)
		_DrawSpine(bounds);

	// The pages cast a soft shadow onto the cover; the tabs lie on top.
	for (int i = 0; i < 4; i++) {
		SetHighColor(Shade(kBinderColor, 0.6f + i * 0.1f));
		float x = fLeftEdge ? bounds.right - i : bounds.left + i;
		StrokeLine(BPoint(x, bounds.top), BPoint(x, bounds.bottom));
	}

	for (int32 i = 0; i < SECTION_COUNT; i++) {
		if (_IsOnThisEdge(i))
			_DrawTab(i);
	}
}


void
TabStrip::_DrawSpine(BRect bounds)
{
	BRect spine(bounds.left, bounds.top, bounds.left + kSpineWidth,
		bounds.bottom);
	SetHighColor(Shade(kBinderColor, 0.85f));
	FillRect(spine);

	// stitching
	SetHighColor(Shade(kBinderColor, 1.7f));
	for (float y = bounds.top + 4; y < bounds.bottom; y += 10) {
		StrokeLine(BPoint(spine.left + 6, y), BPoint(spine.left + 6, y + 4));
	}

	// the fold where the spine meets the front cover
	for (int i = 0; i < 4; i++) {
		SetHighColor(Shade(kBinderColor, 0.7f + i * 0.08f));
		StrokeLine(BPoint(spine.right - i, bounds.top),
			BPoint(spine.right - i, bounds.bottom));
	}
}


void
TabStrip::_DrawTab(int32 index)
{
	BRect tab = _TabRect(index);
	bool selected = index == fSelected;
	rgb_color color = selected
		? kSectionColors[index] : Shade(kSectionColors[index], 0.82f);

	// drop shadow
	SetHighColor(Shade(kBinderColor, 0.55f));
	FillRoundRect(tab.OffsetByCopy(fLeftEdge ? -2 : 2, 2), 7, 7);

	// The tab is rounded on its outer edge and square where it tucks
	// under the page.
	SetHighColor(color);
	FillRoundRect(tab, 7, 7);
	if (fLeftEdge)
		FillRect(BRect(tab.right - 8, tab.top, tab.right, tab.bottom));
	else
		FillRect(BRect(tab.left, tab.top, tab.left + 8, tab.bottom));

	// a highlight along the outer edge, a darker line along the bottom
	SetHighColor(Shade(color, 1.2f));
	float outer = fLeftEdge ? tab.left + 1 : tab.right - 1;
	StrokeLine(BPoint(outer, tab.top + 5), BPoint(outer, tab.bottom - 5));
	SetHighColor(Shade(color, 0.75f));
	StrokeLine(BPoint(tab.left + 4, tab.bottom),
		BPoint(tab.right - 4, tab.bottom));

	// Label, running up the left-hand tabs and down the right-hand ones,
	// as it would on paper dividers.
	BFont font(be_bold_font);
	font.SetSize(12);
	font_height fh;
	font.GetHeight(&fh);
	BString label(kSectionNames[index]);
	font.TruncateString(&label, B_TRUNCATE_END, tab.Height() - 10);
	float width = font.StringWidth(label.String());
	float centerX = (tab.left + tab.right) / 2;
	float centerY = (tab.top + tab.bottom) / 2;
	float half = (fh.ascent - fh.descent) / 2;

	BPoint start;
	if (fLeftEdge) {
		font.SetRotation(90);
		start = BPoint(floorf(centerX + half), floorf(centerY + width / 2));
	} else {
		font.SetRotation(270);
		start = BPoint(floorf(centerX - half), floorf(centerY - width / 2));
	}
#ifdef APP_COSMOE
	// Cosmoe turns rotated text about the top-left corner of its box rather
	// than about the baseline origin, so it lands one ascent off to the
	// side. Move the start point to compensate.
	if (fLeftEdge)
		start += BPoint(-fh.ascent, fh.ascent);
	else
		start += BPoint(fh.ascent, fh.ascent);
#endif
	SetFont(&font);
	SetLowColor(color);
	SetHighColor(kInkColor);
	SetDrawingMode(B_OP_OVER);
	DrawString(label.String(), start);
	SetDrawingMode(B_OP_COPY);
}


void
TabStrip::MouseDown(BPoint where)
{
	for (int32 i = 0; i < SECTION_COUNT; i++) {
		if (!_IsOnThisEdge(i) || !_TabRect(i).Contains(where))
			continue;
		BMessage message(MSG_SELECT_SECTION);
		message.AddInt32("index", i);
		Window()->PostMessage(&message);
		return;
	}
}


// #pragma mark - RingMargin


static const float kRingMarginWidth = 40;
static const float kHeaderHeight = 40;	// see SectionHeader
static const int kRingCount = 6;		// as on the Calendar


RingMargin::RingMargin(int32 section)
	:
	BView("rings", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fSection(section)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	SetExplicitMinSize(BSize(kRingMarginWidth, B_SIZE_UNSET));
	SetExplicitMaxSize(BSize(kRingMarginWidth, B_SIZE_UNLIMITED));
}


void
RingMargin::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	SetHighColor(kPaperColor);
	FillRect(bounds);

	// carry on the coloured rule under the section header
	SetHighColor(kSectionColors[fSection]);
	FillRect(BRect(bounds.left, kHeaderHeight - 4, bounds.right,
		kHeaderHeight - 1));

	float holeX = bounds.left + 27;
	for (int i = 0; i < kRingCount; i++) {
		float y = floorf(bounds.top + (i + 0.5f) * bounds.Height()
			/ kRingCount);

		// punched hole, with the cover showing through
		BRect hole(holeX - 7, y - 7, holeX + 7, y + 7);
		SetHighColor(Shade(kPaperShade, 0.8f));
		FillEllipse(hole.OffsetByCopy(1, 1));
		SetHighColor(Shade(kBinderColor, 0.8f));
		FillEllipse(hole);

		// The ring comes out from under the page edge and into the hole,
		// drawn like the Calendar's rings.
		BRect ring(bounds.left - 8, y - 6, holeX + 2, y + 6);
		SetHighColor(120, 120, 128);
		FillRoundRect(ring.OffsetByCopy(0, 2), 6, 6);
		SetHighColor(205, 205, 215);
		FillRoundRect(ring, 6, 6);
		SetHighColor(250, 250, 255);
		StrokeLine(BPoint(ring.left + 6, ring.top + 2),
			BPoint(ring.right - 6, ring.top + 2));
	}

	// the page edge
	SetHighColor(Shade(kPaperShade, 0.9f));
	StrokeLine(bounds.LeftTop(), bounds.LeftBottom());
}
