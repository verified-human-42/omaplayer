QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release link_pkgconfig qtquickcompiler
PKGCONFIG += mpv
TARGET = omaplayer
TEMPLATE = app

# The one place the version lives; bin/release bumps it.
VERSION = 0.5.0
DEFINES += OMAPLAYER_VERSION=\\\"$$VERSION\\\"

HEADERS += \
    src/actions.h \
    src/desktop.h \
    src/filepicker.h \
    src/mpris.h \
    src/player.h \
    src/portalfilepicker.h \
    src/sami.h \
    src/subtitles.h \
    src/textencoding.h \
    src/settings.h \
    src/theme.h \
    src/timing.h \
    src/translator.h \
    src/videorenderer.h

SOURCES += \
    src/main.cpp \
    src/actions.cpp \
    src/desktop.cpp \
    src/mpris.cpp \
    src/player.cpp \
    src/portalfilepicker.cpp \
    src/sami.cpp \
    src/subtitles.cpp \
    src/textencoding.cpp \
    src/settings.cpp \
    src/theme.cpp \
    src/translator.cpp \
    src/videorenderer.cpp

RESOURCES += src/resources.qrc
