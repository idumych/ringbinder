/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <GraphicsDefs.h>
#include <SupportDefs.h>

#define APP_SIGNATURE "application/x-vnd.Ringbinder"
#define APP_NAME "Ringbinder"

enum {
	MSG_SELECT_SECTION	= 'Ssel',
	MSG_RECORD_SAVED	= 'Rsav',
	MSG_DATA_CHANGED	= 'Dchg',
	MSG_NEW_ENTRY		= 'Enew',
	MSG_EDIT_ENTRY		= 'Eedt',
	MSG_DELETE_ENTRY	= 'Edel',
	MSG_TOGGLE_DONE		= 'Etgl',
	MSG_SAVE_ALL		= 'Fsav',
	MSG_GO_TODAY		= 'Gtdy',
	MSG_PREV			= 'Gprv',
	MSG_NEXT			= 'Gnxt',
	MSG_LIST_SELECTION	= 'Lsel',
	MSG_LETTER			= 'Lltr',
	MSG_EDIT_OK			= 'EdOK',
	MSG_ABOUT			= 'Abou',

	// Google sync
	MSG_SYNC_NOW			= 'Gsyn',
	MSG_SYNC_TIMER			= 'Gtmr',
	MSG_SYNC_DONE			= 'Gdon',
	MSG_OPEN_SYNC_SETTINGS	= 'Gset',
	MSG_GOOGLE_SETTINGS		= 'Gcfg',	// settings window -> binder window
	MSG_GOOGLE_CONNECT		= 'Gcon',
	MSG_GOOGLE_CANCEL		= 'Gcan',
	MSG_GOOGLE_CONNECTED	= 'Gok!',	// sign-in thread -> binder window
	MSG_GOOGLE_DISCONNECT	= 'Gdis',
	MSG_GOOGLE_STATUS		= 'Gsta'	// binder window -> settings window
};

enum SectionIndex {
	SECTION_CALENDAR = 0,
	SECTION_TODO,
	SECTION_ADDRESS,
	SECTION_CALLS,
	SECTION_PLANNER,
	SECTION_NOTEPAD,
	SECTION_ANNIVERSARY,
	SECTION_COUNT
};

extern const char* kSectionKeys[SECTION_COUNT];
extern const char* kSectionNames[SECTION_COUNT];
extern const rgb_color kSectionColors[SECTION_COUNT];

extern const rgb_color kPaperColor;
extern const rgb_color kPaperShade;
extern const rgb_color kRuleColor;
extern const rgb_color kInkColor;
extern const rgb_color kBinderColor;

#endif // CONSTANTS_H
