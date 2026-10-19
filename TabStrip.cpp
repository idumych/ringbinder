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


static rgb_color
Shade(rgb_color color, float factor)
{
	return make_color((uint8)std::min(255.0f, color.red * factor),
		(uint8)std::min(255.0f, color.green * factor),
		(uint8)std::min(255.0f, color.blue * factor));
}


TabStrip::TabStrip()
	:
	BView("tabs", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fSelected(0)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	SetExplicitMinSize(BSize(112, SECTION_COUNT * 40 + 40));
	SetExplicitMaxSize(BSize(112, B_SIZE_UNLIMITED));
}


void
TabStrip::SetSelected(int32 index)
{
	if (index == fSelected)
		return;
	fSelected = index;
	Invalidate();
}


BRect
TabStrip::_TabRect(int32 index) const
{
	BRect bounds = Bounds();
	float height = std::min(58.0f,
		floorf((bounds.Height() - 40) / SECTION_COUNT) - 6);
	float top = floorf(bounds.top + 20 + index * (height + 6));
	float left = index == fSelected ? bounds.left : bounds.left + 6;
	return BRect(left, top, bounds.right - 10, top + height);
}


void
TabStrip::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	SetHighColor(kBinderColor);
	FillRect(bounds);

	BFont font(be_bold_font);
	font.SetSize(12);
	SetFont(&font);
	font_height fh;
	font.GetHeight(&fh);

	for (int32 i = 0; i < SECTION_COUNT; i++) {
		BRect tab = _TabRect(i);
		bool selected = i == fSelected;
		rgb_color color = selected
			? kSectionColors[i] : Shade(kSectionColors[i], 0.80f);

		// drop shadow
		SetHighColor(Shade(kBinderColor, 0.6f));
		FillRoundRect(tab.OffsetByCopy(2, 2), 8, 8);

		SetHighColor(color);
		FillRoundRect(tab, 8, 8);
		// square off the edge that tucks under the page
		FillRect(BRect(tab.left, tab.top, tab.left + 9, tab.bottom));

		// highlight along the top edge
		SetHighColor(Shade(color, 1.18f));
		StrokeLine(BPoint(tab.left + 2, tab.top + 1),
			BPoint(tab.right - 6, tab.top + 1));

		SetLowColor(color);
		SetHighColor(kInkColor);
		const char* label = kSectionNames[i];
		float width = font.StringWidth(label);
		DrawString(label, BPoint(
			floorf(tab.left + (tab.Width() - width) / 2 + 3),
			floorf(tab.top + (tab.Height() + fh.ascent - fh.descent) / 2)));
	}
}


void
TabStrip::MouseDown(BPoint where)
{
	for (int32 i = 0; i < SECTION_COUNT; i++) {
		if (!_TabRect(i).Contains(where))
			continue;
		BMessage message(MSG_SELECT_SECTION);
		message.AddInt32("index", i);
		Window()->PostMessage(&message);
		return;
	}
}


// #pragma mark - BinderSpine


BinderSpine::BinderSpine()
	:
	BView("spine", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	SetExplicitMinSize(BSize(24, B_SIZE_UNSET));
	SetExplicitMaxSize(BSize(24, B_SIZE_UNLIMITED));
}


void
BinderSpine::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	SetHighColor(kBinderColor);
	FillRect(bounds);

	// stitching
	SetHighColor(Shade(kBinderColor, 1.7f));
	for (float y = bounds.top + 4; y < bounds.bottom; y += 10) {
		StrokeLine(BPoint(bounds.left + 6, y),
			BPoint(bounds.left + 6, y + 4));
	}

	// shadow where the pages meet the cover
	for (int i = 0; i < 6; i++) {
		SetHighColor(Shade(kBinderColor, 0.9f - i * 0.08f));
		StrokeLine(BPoint(bounds.right - i, bounds.top),
			BPoint(bounds.right - i, bounds.bottom));
	}
}
