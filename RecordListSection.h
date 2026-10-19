/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * A configurable list-style page used by To Do, Address, Calls and
 * Anniversary.
 */
#ifndef RECORD_LIST_SECTION_H
#define RECORD_LIST_SECTION_H

#include "Section.h"

class BListView;
class BStringView;
class LetterBar;

enum ColumnFormat {
	COL_TEXT,
	COL_CHECK,
	COL_DATE,
	COL_MONTHDAY,
	COL_COUNTDOWN	// days until the next yearly occurrence of "date"
};

struct ColumnSpec {
	const char*	key;
	const char*	title;
	float		width;	// 0 = take the remaining space
	int32		format;
};

struct ListConfig {
	const FieldSpec*	fields;
	int32				fieldCount;
	const ColumnSpec*	columns;
	int32				columnCount;
	const char*			sortKeys[4];	// NULL terminated
	bool				monthDaySort;	// sort by month/day of "date" first
	const char*			doneKey;		// bool field shown struck out
	const char*			letterKey;		// show an A-Z index on this field
	const char*			singular;		// "Contact"
	const char*			titleKey;		// used in delete confirmation
	void				(*defaults)(BMessage& record);
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

			ListConfig		fConfig;
			BListView*		fList;
			BStringView*	fCount;
			LetterBar*		fLetterBar;
			BString			fLetter;
};

#endif // RECORD_LIST_SECTION_H
