/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * The coloured section tabs along the edges of the binder. Like the
 * dividers in a real ring binder, the tabs of the sections you have
 * turned past sit on the left-hand edge (with the open section's tab
 * last), and the sections still to come stay on the right. Each tab keeps
 * its height, so turning to a section moves its tab across the binder.
 * The left-hand strip also draws the leather spine.
 */
#ifndef TAB_STRIP_H
#define TAB_STRIP_H

#include <View.h>


class TabStrip : public BView {
public:
							TabStrip(bool leftEdge);

			void			SetSelected(int32 index);
			int32			Selected() const { return fSelected; }

	virtual	void			Draw(BRect updateRect);
	virtual	void			MouseDown(BPoint where);

private:
			bool			_IsOnThisEdge(int32 index) const;
			BRect			_TabRect(int32 index) const;
			void			_DrawSpine(BRect bounds);
			void			_DrawTab(int32 index);

			bool			fLeftEdge;
			int32			fSelected;
};


// The punched left margin of the Planner's single full-width page, with
// the binder's rings through it, as on a paper organizer's year planner.
// (The other sections are two-page spreads with the rings down the
// middle.)
class RingMargin : public BView {
public:
							RingMargin(int32 section);
	virtual	void			Draw(BRect updateRect);

private:
			int32			fSection;
};

#endif // TAB_STRIP_H
