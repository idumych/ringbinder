/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "Constants.h"

const char* kSectionKeys[SECTION_COUNT] = {
	"calendar", "todo", "address", "calls", "planner", "notepad",
	"anniversary"
};

const char* kSectionNames[SECTION_COUNT] = {
	"Calendar", "To Do", "Address", "Calls", "Planner", "Notepad",
	"Anniversary"
};

const rgb_color kSectionColors[SECTION_COUNT] = {
	{ 122, 160, 214, 255 },	// Calendar
	{ 236, 162,  92, 255 },	// To Do
	{ 132, 190, 118, 255 },	// Address
	{ 198, 132, 182, 255 },	// Calls
	{ 226, 206,  96, 255 },	// Planner
	{ 140, 196, 196, 255 },	// Notepad
	{ 214, 112, 112, 255 }	// Anniversary
};

const rgb_color kPaperColor		= { 253, 250, 236, 255 };
const rgb_color kPaperShade		= { 238, 232, 212, 255 };
const rgb_color kRuleColor		= { 200, 214, 234, 255 };
const rgb_color kInkColor		= {  30,  32,  64, 255 };
const rgb_color kBinderColor	= {  70,  42,  34, 255 };
