/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * Field and column layouts for the list-style sections, and the factory
 * that builds every page of the binder.
 */
#include "Section.h"

#include <time.h>

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

static const ColumnSpec kTodoColumns[] = {
	{ "done",		"",			26,		COL_CHECK },
	{ "priority",	"Priority",	90,		COL_TEXT },
	{ "title",		"Task",		260,	COL_TEXT },
	{ "due",		"Due",		110,	COL_DATE },
	{ "category",	"Category",	100,	COL_TEXT },
	{ "notes",		"Notes",	0,		COL_TEXT }
};

static const ListConfig kTodoConfig = {
	kTodoFields, COUNT_OF(kTodoFields),
	kTodoColumns, COUNT_OF(kTodoColumns),
	{ "priority", "due", "title", NULL },
	false, "done", NULL, "Task", "title", NULL
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
	{ "last",		"Last name",	130,	COL_TEXT },
	{ "first",		"First name",	110,	COL_TEXT },
	{ "company",	"Company",		150,	COL_TEXT },
	{ "phone",		"Phone",		120,	COL_TEXT },
	{ "mobile",		"Mobile",		120,	COL_TEXT },
	{ "email",		"E-mail",		0,		COL_TEXT }
};

static const ListConfig kAddressConfig = {
	kAddressFields, COUNT_OF(kAddressFields),
	kAddressColumns, COUNT_OF(kAddressColumns),
	{ "last", "first", "company", NULL },
	false, NULL, "last", "Contact", "last", NULL
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
	{ "done",		"",			26,		COL_CHECK },
	{ "date",		"Date",		105,	COL_DATE },
	{ "time",		"Time",		55,		COL_TEXT },
	{ "direction",	"Type",		85,		COL_TEXT },
	{ "name",		"Name",		160,	COL_TEXT },
	{ "phone",		"Phone",	120,	COL_TEXT },
	{ "notes",		"Notes",	0,		COL_TEXT }
};

static const ListConfig kCallConfig = {
	kCallFields, COUNT_OF(kCallFields),
	kCallColumns, COUNT_OF(kCallColumns),
	{ "date", "time", "name", NULL },
	false, "done", NULL, "Call", "name", CallDefaults
};


// #pragma mark - Anniversary


static const FieldSpec kAnniversaryFields[] = {
	{ "title",		"Occasion",		FIELD_TEXT,		NULL, true },
	{ "date",		"Date",			FIELD_DATE,		NULL, true },
	{ "category",	"Category",		FIELD_CHOICE,
		"Birthday|Anniversary|Holiday|Other", false },
	{ "notes",		"Notes",		FIELD_MULTILINE, NULL, false }
};

static const ColumnSpec kAnniversaryColumns[] = {
	{ "date",		"Date",		130,	COL_MONTHDAY },
	{ "title",		"Occasion",	260,	COL_TEXT },
	{ "category",	"Category",	110,	COL_TEXT },
	{ "date",		"Next",		100,	COL_COUNTDOWN },
	{ "notes",		"Notes",	0,		COL_TEXT }
};

static const ListConfig kAnniversaryConfig = {
	kAnniversaryFields, COUNT_OF(kAnniversaryFields),
	kAnniversaryColumns, COUNT_OF(kAnniversaryColumns),
	{ "title", NULL, NULL, NULL },
	true, NULL, NULL, "Occasion", "title", NULL
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
			return new RecordListSection(index, store, kAnniversaryConfig);
	}
}
