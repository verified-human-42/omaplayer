QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release link_pkgconfig qtquickcompiler
PKGCONFIG += mpv
TARGET = omaplayer
TEMPLATE = app

HEADERS += \
    src/actions.h \
    src/filepicker.h \
    src/player.h \
    src/portalfilepicker.h \
    src/settings.h \
    src/theme.h \
    src/timing.h \
    src/translator.h \
    src/videorenderer.h

SOURCES += \
    src/main.cpp \
    src/actions.cpp \
    src/player.cpp \
    src/portalfilepicker.cpp \
    src/settings.cpp \
    src/theme.cpp \
    src/translator.cpp \
    src/videorenderer.cpp

RESOURCES += src/resources.qrc
