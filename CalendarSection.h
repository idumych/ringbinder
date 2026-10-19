/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * Two-page week-at-a-glance diary: Mon-Wed on the left page, Thu-Sun on the
 * right, with the binder rings in between.
 */
#ifndef CALENDAR_SECTION_H
#define CALENDAR_SECTION_H

#include "DateUtil.h"
#include "Section.h"

class BStringView;
class WeekView;


class CalendarSection : public Section {
public:
							CalendarSection(int32 index, DataStore* store);

	virtual	void			MessageReceived(BMessage* message);

	virtual	void			Reload();
	virtual	void			NewEntry();
	virtual	void			EditSelected();
	virtual	void			DeleteSelected();

private:
			void			_NewEntryOn(const SimpleDate& date);
			void			_UpdateLabel();

			WeekView*		fWeek;
			BStringView*	fLabel;
};

#endif // CALENDAR_SECTION_H
