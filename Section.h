/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * Base class for one "page" of the binder (Calendar, To Do, ...).
 */
#ifndef SECTION_H
#define SECTION_H

#include <View.h>

#include "Constants.h"
#include "DataStore.h"
#include "EditWindow.h"

class BButton;


class SectionHeader : public BView {
public:
							SectionHeader(int32 index);
	virtual	void			Draw(BRect updateRect);

private:
			int32			fIndex;
};


class Section : public BView {
public:
							Section(const char* name, int32 index,
								DataStore* store);

	virtual	void			AllAttached();
	virtual	void			MessageReceived(BMessage* message);

	virtual	void			Reload() = 0;
	virtual	void			NewEntry() {}
	virtual	void			EditSelected() {}
	virtual	void			DeleteSelected() {}
	virtual	void			ToggleSelected() {}
	virtual	void			Commit() {}

			const char*		Key() const { return kSectionKeys[fIndex]; }
			int32			Index() const { return fIndex; }

protected:
			void			OpenEditor(const char* title,
								const FieldSpec* fields, int32 count,
								const BMessage& record);
			bool			ConfirmDelete(const char* what);
			void			DataChanged();
	static	BButton*		MakeButton(const char* label, uint32 what);

			DataStore*		fStore;
			int32			fIndex;
};


// Implemented in SectionFactory.cpp
Section* CreateSection(int32 index, DataStore* store);

#endif // SECTION_H
