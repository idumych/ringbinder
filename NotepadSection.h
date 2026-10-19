/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * Notepad: a table of contents on the left, the page itself on the right.
 * Edits are committed when you change page, switch section or quit.
 */
#ifndef NOTEPAD_SECTION_H
#define NOTEPAD_SECTION_H

#include "Section.h"

class BListView;
class BTextControl;
class BTextView;


class NotepadSection : public Section {
public:
							NotepadSection(int32 index, DataStore* store);

	virtual	void			MessageReceived(BMessage* message);

	virtual	void			Reload();
	virtual	void			NewEntry();
	virtual	void			EditSelected();
	virtual	void			DeleteSelected();
	virtual	void			Commit();

private:
			void			_LoadPage(int64 id);
			int64			_SelectedId() const;

			BListView*		fList;
			BTextControl*	fTitle;
			BTextView*		fBody;
			int64			fCurrentId;
};

#endif // NOTEPAD_SECTION_H
