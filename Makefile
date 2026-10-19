## Haiku makefile-engine build for Ringbinder
## Build with:  make        (output in objects.*/Ringbinder)

NAME = Ringbinder
TYPE = APP
APP_MIME_SIG = application/x-vnd.Ringbinder

SRCS = \
	App.cpp \
	BinderWindow.cpp \
	GoogleSettingsWindow.cpp \
	GoogleSync.cpp \
	CalendarSection.cpp \
	Constants.cpp \
	DataStore.cpp \
	DateUtil.cpp \
	EditWindow.cpp \
	NotepadSection.cpp \
	PlannerSection.cpp \
	RecordListSection.cpp \
	Section.cpp \
	SectionFactory.cpp \
	TabStrip.cpp \
	sync/CurlTransport.cpp \
	sync/GoogleAuth.cpp \
	sync/Http.cpp \
	sync/Json.cpp \
	sync/SyncCore.cpp

RDEFS = Ringbinder.rdef

LIBS = be network curl $(STDCPPLIBS)

OPTIMIZE := FULL
WARNINGS = ALL
SYMBOLS :=
DEBUGGER :=
COMPILER_FLAGS = -std=c++11

## Include the makefile-engine
DEVEL_DIRECTORY := \
	$(shell findpaths -r "makefile_engine" B_FIND_PATH_DEVELOP_DIRECTORY)
include $(DEVEL_DIRECTORY)/etc/makefile-engine

## Haiku package, built on Haiku R1/beta6+ (see packaging/haiku/build-hpkg.sh)
hpkg:
	sh packaging/haiku/build-hpkg.sh

.PHONY: hpkg
