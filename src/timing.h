#pragma once

#include <QLoggingCategory>

// Startup and first-frame timestamps (milliseconds since the epoch), off by
// default: QT_LOGGING_RULES="omaplayer.timing.info=true" omaplayer FILE
Q_DECLARE_LOGGING_CATEGORY(lcTiming)
