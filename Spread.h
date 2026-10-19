/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * An open two-page spread of the binder: two paper pages on the leather
 * cover, with a gap between them and the metal rings across it. Sections
 * put their views on the left and right pages.
 */
#ifndef SPREAD_H
#define SPREAD_H

#include <GroupView.h>


class SpreadView : public BView {
public:
							SpreadView();

			BGroupView*		LeftPage() const { return fLeft; }
			BGroupView*		RightPage() const { return fRight; }

	virtual	void			Draw(BRect updateRect);

private:
			BGroupView*		fLeft;
			BGroupView*		fRight;
};


// Draws the six binder rings across the gap at "middle", as on the
// Calendar's week spread.
void DrawBinderRings(BView* view, BRect bounds, float middle);

#endif // SPREAD_H
