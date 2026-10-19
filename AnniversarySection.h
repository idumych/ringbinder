/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * Birthdays and other yearly dates, laid out like the anniversary pages
 * of a paper organizer: half a year per spread, three months to a page.
 */
#ifndef ANNIVERSARY_SECTION_H
#define ANNIVERSARY_SECTION_H

#include "DateUtil.h"
#include "Section.h"

class BStringView;
class MonthPage;


class AnniversarySection : public Section {
public:
							AnniversarySection(int32 index,
								DataStore* store);

	virtual	void			MessageReceived(BMessage* message);

	virtual	void			Reload();
	virtual	void			NewEntry();
	virtual	void			EditSelected();
	virtual	void			DeleteSelected();

			int				Year() const { return fYear; }
			int				FirstMonth() const { return fFirstMonth; }
			int64			SelectedId() const { return fSelectedId; }
			void			Select(int64 id);

private:
			void			_ShowHalf(int year, int firstMonth);
			void			_NewOn(const SimpleDate& date);
			void			_Refresh();

			int				fYear;
			int				fFirstMonth;	// 1 or 7
			int64			fSelectedId;
			MonthPage*		fLeft;
			MonthPage*		fRight;
			BStringView*	fLabel;
};

#endif // ANNIVERSARY_SECTION_H
