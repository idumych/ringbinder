/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * Field and column layouts for the list-style sections, and the factory
 * that builds every page of the binder.
 */
#include "Section.h"

#include <time.h>

#include "AnniversarySection.h"
#include "CalendarSection.h"
#include "DateUtil.h"
#include "NotepadSection.h"
#include "PlannerSection.h"
#include "RecordListSection.h"

#define COUNT_OF(array) ((int32)(sizeof(array) / sizeof(array[0])))


// #pragma mark - To Do


static const FieldSpec kTodoFields[] = {
	{ "title",		"Task",			FIELD_TEXT,		NULL, true },
	{ "priority",	"Priority",		FIELD_CHOICE,
		"2 - Normal|1 - High|3 - Low", false },
	{ "due",		"Due date",		FIELD_DATE,		NULL, false },
	{ "category",	"Category",		FIELD_CHOICE,
		"Business|Personal|Home|Errands|Other", false },
	{ "done",		"Completed",	FIELD_CHECK,	NULL, false },
	{ "notes",		"Notes",		FIELD_MULTILINE, NULL, false }
};

// The list fills the left-hand page; the card on the right shows the rest.
static const ColumnSpec kTodoColumns[] = {
	{ "done",		"",			24,		COL_CHECK },
	{ "priority",	"Pri",		34,		COL_SHORT },
	{ "title",		"Task",		0,		COL_TEXT },
	{ "due",		"Due",		96,		COL_DATE }
};

static const ListConfig kTodoConfig = {
	kTodoFields, COUNT_OF(kTodoFields),
	kTodoColumns, COUNT_OF(kTodoColumns),
	{ "priority", "due", "title", NULL },
	false, "done", NULL, "Task", "title", NULL,
	INDEX_TODO, { "title", NULL, NULL }
};


// #pragma mark - Address


static const FieldSpec kAddressFields[] = {
	{ "last",		"Last name",	FIELD_TEXT,		NULL, false },
	{ "first",		"First name",	FIELD_TEXT,		NULL, false },
	{ "company",	"Company",		FIELD_TEXT,		NULL, false },
	{ "jobtitle",	"Job title",	FIELD_TEXT,		NULL, false },
	{ "phone",		"Phone",		FIELD_TEXT,		NULL, false },
	{ "mobile",		"Mobile",		FIELD_TEXT,		NULL, false },
	{ "email",		"E-mail",		FIELD_TEXT,		NULL, false },
	{ "address",	"Address",		FIELD_MULTILINE, NULL, false },
	{ "birthday",	"Birthday",		FIELD_DATE,		NULL, false },
	{ "notes",		"Notes",		FIELD_MULTILINE, NULL, false }
};

static const ColumnSpec kAddressColumns[] = {
	{ "last",		"Last name",	0,		COL_TEXT },
	{ "first",		"First name",	110,	COL_TEXT },
	{ "phone",		"Phone",		110,	COL_TEXT }
};

static const ListConfig kAddressConfig = {
	kAddressFields, COUNT_OF(kAddressFields),
	kAddressColumns, COUNT_OF(kAddressColumns),
	{ "last", "first", "company", NULL },
	false, NULL, "last", "Contact", "last", NULL,
	INDEX_LETTERS, { "first", "last", NULL }
};


// #pragma mark - Calls


static void
CallDefaults(BMessage& record)
{
	SetString(record, "date", FormatDate(Today()).String());
	time_t now = time(NULL);
	struct tm local;
	localtime_r(&now, &local);
	SetString(record, "time",
		FormatTime(local.tm_hour, local.tm_min).String());
}

static const FieldSpec kCallFields[] = {
	{ "name",		"Name",			FIELD_TEXT,		NULL, true },
	{ "company",	"Company",		FIELD_TEXT,		NULL, false },
	{ "phone",		"Phone",		FIELD_TEXT,		NULL, false },
	{ "direction",	"Type",			FIELD_CHOICE,
		"Outgoing|Incoming|Missed|Call back", false },
	{ "date",		"Date",			FIELD_DATE,		NULL, false },
	{ "time",		"Time",			FIELD_TIME,		NULL, false },
	{ "done",		"Completed",	FIELD_CHECK,	NULL, false },
	{ "notes",		"Notes",		FIELD_MULTILINE, NULL, false }
};

static const ColumnSpec kCallColumns[] = {
	{ "done",		"",			24,		COL_CHECK },
	{ "date",		"Date",		96,		COL_DATE },
	{ "time",		"Time",		50,		COL_TEXT },
	{ "name",		"Name",		0,		COL_TEXT },
	{ "direction",	"Type",		76,		COL_TEXT }
};

static const ListConfig kCallConfig = {
	kCallFields, COUNT_OF(kCallFields),
	kCallColumns, COUNT_OF(kCallColumns),
	{ "date", "time", "name", NULL },
	false, "done", NULL, "Call", "name", CallDefaults,
	INDEX_NONE, { "name", NULL, NULL }
};


// #pragma mark - factory


Section*
CreateSection(int32 index, DataStore* store)
{
	switch (index) {
		case SECTION_CALENDAR:
			return new CalendarSection(index, store);
		case SECTION_TODO:
			return new RecordListSection(index, store, kTodoConfig);
		case SECTION_ADDRESS:
			return new RecordListSection(index, store, kAddressConfig);
		case SECTION_CALLS:
			return new RecordListSection(index, store, kCallConfig);
		case SECTION_PLANNER:
			return new PlannerSection(index, store);
		case SECTION_NOTEPAD:
			return new NotepadSection(index, store);
		case SECTION_ANNIVERSARY:
		default:
			return new AnniversarySection(index, store);
	}
}
