QT += core gui quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase link_pkgconfig
PKGCONFIG += mpv
TARGET = omaplayer_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/actions.h ../src/player.h ../src/settings.h ../src/theme.h ../src/timing.h ../src/translator.h
SOURCES += omaplayer_tests.cpp ../src/actions.cpp ../src/player.cpp ../src/settings.cpp ../src/theme.cpp ../src/translator.cpp
