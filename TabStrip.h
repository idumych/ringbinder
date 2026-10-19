/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * The coloured section tabs down the right edge of the binder, and the
 * leather spine on the left.
 */
#ifndef TAB_STRIP_H
#define TAB_STRIP_H

#include <View.h>


class TabStrip : public BView {
public:
							TabStrip();

			void			SetSelected(int32 index);
			int32			Selected() const { return fSelected; }

	virtual	void			Draw(BRect updateRect);
	virtual	void			MouseDown(BPoint where);

private:
			BRect			_TabRect(int32 index) const;

			int32			fSelected;
};


class BinderSpine : public BView {
public:
							BinderSpine();
	virtual	void			Draw(BRect updateRect);
};

#endif // TAB_STRIP_H
