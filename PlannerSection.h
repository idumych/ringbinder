/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * Year-at-a-glance planner: 12 month rows by 37 weekday columns, with
 * colour coded blocks for vacations, trips, training and so on.
 */
#ifndef PLANNER_SECTION_H
#define PLANNER_SECTION_H

#include "DateUtil.h"
#include "Section.h"

class BStringView;
class YearView;


class PlannerSection : public Section {
public:
							PlannerSection(int32 index, DataStore* store);

	virtual	void			MessageReceived(BMessage* message);

	virtual	void			Reload();
	virtual	void			NewEntry();
	virtual	void			EditSelected();
	virtual	void			DeleteSelected();

private:
			void			_NewEvent(const SimpleDate& start,
								const SimpleDate& end);
			void			_UpdateLabels();

			YearView*		fYear;
			BStringView*	fYearLabel;
			BStringView*	fStatus;
};

#endif // PLANNER_SECTION_H
