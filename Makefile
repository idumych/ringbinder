## Ringbinder: builds on Haiku/Renku OS, on VitruvianOS (V\OS) and on plain
## Linux (through Cosmoe) from the same sources.
##
##   make          -> Haiku: objects.*/Ringbinder   V\OS, Linux: ./Ringbinder
##   make hpkg     -> Haiku package (see packaging/haiku/build-hpkg.sh)
##   make deb      -> V\OS Debian package (see packaging/build-deb.sh)
##   make appimage -> Linux AppImage (see packaging/appimage/build-appimage.sh)
##   make test     -> sync engine unit tests (any OS, no Haiku API needed)

NAME = Ringbinder

SRCS = \
	AnniversarySection.cpp \
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
	Spread.cpp \
	TabStrip.cpp \
	sync/CurlTransport.cpp \
	sync/GoogleAuth.cpp \
	sync/Http.cpp \
	sync/Json.cpp \
	sync/SyncCore.cpp

ifeq ($(shell uname -s),Haiku)
## ---- Haiku / Renku OS: the makefile-engine ------------------------------

TYPE = APP
APP_MIME_SIG = application/x-vnd.Ringbinder

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

else
## ---- VitruvianOS: a BeOS-like OS based on Linux ---------------------------

CXX      ?= g++
CXXFLAGS ?= -O2 -g
override CXXFLAGS += -std=c++11 -Wall -Wno-multichar
VOS_HEADERS ?= /system/develop/headers
COSMOE_ROOT ?= $(CURDIR)/.cosmoe/stage/usr
VOS_ARCH := $(if $(filter aarch64,$(shell uname -m)),arm64,$(shell uname -m))

# Headers and flags come from the vos-dev package through pkg-config, or
# from the link-time SDK (tools/vos-sdk/make-sdk.sh, then
# ". .vos-sdk/sdk/env.sh").
ifeq ($(shell pkg-config --exists vos && echo yes),yes)
  PLATFORM    := VitruvianOS
  override CXXFLAGS += $(shell pkg-config --cflags vos) -fPIC
  LIBS        := $(shell pkg-config --libs vos) -ltracker -ltranslation -lcurl
  RESOURCES    = $(VOS_RESOURCES)
else ifneq ($(wildcard $(VOS_HEADERS)/os/BeBuild.h),)
  # VitruvianOS 0.6.x has no vos-dev/pkg-config yet: the headers ship in the
  # system package under /system/develop/headers.
  PLATFORM    := VitruvianOS
  VOS_DIRS    := . os os/app os/drivers os/kernel os/support os/storage \
                 os/translation os/config os/add-ons/file_system \
                 os/add-ons/graphics os/add-ons/registrar \
                 os/add-ons/network_settings os/arch/$(VOS_ARCH) os/device \
                 os/drivers/bus os/game os/interface os/locale \
                 os/locale/tools os/mail os/net os/posix/compat/sys \
                 os/add-ons/input_server os/add-ons/tracker os/opengl \
                 os/media2 posix build build/config_headers config
  override CXXFLAGS += $(addprefix -I$(VOS_HEADERS)/,$(VOS_DIRS)) \
                 -include LinuxBuildCompatibility.h -fPIC
  LIBS        := -lbe -lroot -ltracker -ltranslation -lcurl
  RESOURCES    = $(VOS_RESOURCES)
else ifneq ($(wildcard $(COSMOE_ROOT)/include/os/app/Application.h),)
  # Plain Linux with Cosmoe (the Haiku API as a library on X11/Wayland).
  # COSMOE_ROOT is its install prefix; build-appimage.sh points it at a
  # staged copy that ships inside the AppImage.
  PLATFORM    := Cosmoe
  COSMOE_DIRS := config os os/app os/game os/interface os/kernel os/locale \
                 os/media os/net os/storage os/support os/translation \
                 os/be_apps/Tracker private/interface private/shared \
                 private/support posix
  override CXXFLAGS += -I$(COSMOE_ROOT)/include \
                 $(addprefix -I$(COSMOE_ROOT)/include/,$(COSMOE_DIRS)) \
                 -DAPP_COSMOE -fPIC
  # libbe finds Cosmoe's add-ons and data next to itself, so the AppImage
  # (bin/Ringbinder + lib/) works wherever it is mounted.
  COSMOE_RPATH ?= $$ORIGIN/../lib
  LIBS        := -L$(COSMOE_ROOT)/lib -lbe -ltracker -lcurl \
                 -Wl,-rpath,'$(COSMOE_RPATH)'
  # Cosmoe reads resources from a .cosmoe_rsrc section linked into the app.
  EXTRA_OBJS  := obj/Ringbinder.rsrc.o
  RESOURCES    = true
else
  PLATFORM    := none
endif

# V\OS keeps the app signature both as resources and as attributes
# (absolute paths, as V\OS's own build passes them to xres/resattr)
VOS_RESOURCES = rc -o $(CURDIR)/obj/Ringbinder.rsrc $(CURDIR)/Ringbinder.rdef \
	&& xres -o $(CURDIR)/$@ $(CURDIR)/obj/Ringbinder.rsrc \
	&& resattr -O -o $(CURDIR)/$@ $(CURDIR)/obj/Ringbinder.rsrc

OBJS := $(patsubst %.cpp,obj/%.o,$(SRCS))

all: check-platform $(NAME)

check-platform:
ifeq ($(PLATFORM),none)
	@echo "Ringbinder needs Haiku, VitruvianOS, or Linux with Cosmoe. On"
	@echo "VitruvianOS it uses 'pkg-config vos' (vos-dev) or the headers in"
	@echo "$(VOS_HEADERS); on Linux, Cosmoe installed under COSMOE_ROOT=$(COSMOE_ROOT)"
	@echo "(sh packaging/appimage/build-appimage.sh builds one). To build the"
	@echo "V\\OS package on another Linux, build the link-time SDK with"
	@echo "tools/vos-sdk/make-sdk.sh and source .vos-sdk/sdk/env.sh."
	@echo "(The sync engine tests run anywhere: make test)"
	@false
endif

obj/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

obj/Ringbinder.rsrc.o: Ringbinder.rdef
	@mkdir -p obj
	LD_LIBRARY_PATH=$(COSMOE_ROOT)/lib $(COSMOE_ROOT)/bin/rc \
		-o obj/Ringbinder.rsrc Ringbinder.rdef
	cd obj && ld -r -b binary Ringbinder.rsrc -o Ringbinder.rsrc.o
	objcopy --rename-section .data=.cosmoe_rsrc \
		--add-section .note.GNU-stack=/dev/null \
		--set-section-flags .note.GNU-stack=noload,readonly obj/Ringbinder.rsrc.o

$(NAME): $(OBJS) $(EXTRA_OBJS) Ringbinder.rdef
	@mkdir -p obj
	$(CXX) $(LDFLAGS) $(OBJS) $(EXTRA_OBJS) -o $@ $(LIBS)
	$(RESOURCES)

# Debian package for VitruvianOS (see packaging/build-deb.sh)
deb: all
	sh packaging/build-deb.sh

# AppImage for Linux, built with Cosmoe (see packaging/appimage/build-appimage.sh)
appimage:
	sh packaging/appimage/build-appimage.sh

test:
	sh sync/tests/run_tests.sh

clean:
	rm -rf obj $(NAME)

-include $(OBJS:.o=.d)

.PHONY: all check-platform deb appimage test clean
endif

## Haiku package, built on Haiku R1/beta6+ (see packaging/haiku/build-hpkg.sh)
hpkg:
	sh packaging/haiku/build-hpkg.sh

.PHONY: hpkg
