#!/bin/sh
# Builds and runs the sync engine tests natively (no Haiku needed).
cd "$(dirname "$0")"
# On Haiku the socket functions live in libnetwork, not libc.
NETLIB=
[ "$(uname -s)" = Haiku ] && NETLIB=-lnetwork
c++ -std=c++11 -Wall -Wextra -o sync_test sync_test.cpp ../SyncCore.cpp \
	../Json.cpp ../Http.cpp ../GoogleAuth.cpp $NETLIB && ./sync_test
