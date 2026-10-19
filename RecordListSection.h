/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * A configurable list-style section used by To Do, Address and Calls, laid
 * out as an open two-page spread: the list on the left-hand page (with
 * index tabs down its edge, where the section has them) and the selected
 * entry as a card on the right-hand page.
 */
#ifndef RECORD_LIST_SECTION_H
#define RECORD_LIST_SECTION_H

#include "Section.h"

class BListView;
class BStringView;
class DetailCard;
class IndexTabs;

enum ColumnFormat {
	COL_TEXT,
	COL_CHECK,
	COL_DATE,
	COL_MONTHDAY,
	COL_COUNTDOWN,	// days until the next yearly occurrence of "date"
	COL_SHORT		// the part before " - ", e.g. "2" of "2 - Normal"
};

struct ColumnSpec {
	const char*	key;
	const char*	title;
	float		width;	// 0 = whatever the other columns leave
	int32		format;
};

enum IndexKind {
	INDEX_NONE,
	INDEX_LETTERS,	// All, A-Z on "letterKey"
	INDEX_TODO		// All, Overdue, Current, Future, Completed
};

struct ListConfig {
	const FieldSpec*	fields;
	int32				fieldCount;
	const ColumnSpec*	columns;
	int32				columnCount;
	const char*			sortKeys[4];	// NULL terminated
	bool				monthDaySort;	// sort by month/day of "date" first
	const char*			doneKey;		// bool field shown struck out
	const char*			letterKey;		// field the A-Z tabs look at
	const char*			singular;		// "Contact"
	const char*			titleKey;		// used in delete confirmation
	void				(*defaults)(BMessage& record);
	int32				index;			// IndexKind
	const char*			cardTitleKeys[3];	// joined for the card's heading
};


class RecordListSection : public Section {
public:
							RecordListSection(int32 index, DataStore* store,
								const ListConfig& config);

	virtual	void			MessageReceived(BMessage* message);

	virtual	void			Reload();
	virtual	void			NewEntry();
	virtual	void			EditSelected();
	virtual	void			DeleteSelected();
	virtual	void			ToggleSelected();

private:
			const BMessage*	_SelectedRecord();
			bool			_Matches(const BMessage& record) const;
			void			_UpdateCard();

			ListConfig		fConfig;
			BListView*		fList;
			BStringView*	fCount;
			IndexTabs*		fTabs;
			DetailCard*		fCard;
			int32			fTab;
			int32			fVisible;
};

#endif // RECORD_LIST_SECTION_H
