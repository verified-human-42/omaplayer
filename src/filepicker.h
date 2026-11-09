#pragma once

#include <QList>
#include <QObject>
#include <QUrl>

// Where open dialogs come from: the desktop portal in the app, a fake in tests.
class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~FilePicker() = default;

    // append adds the picks to the playlist instead of playing them now.
    Q_INVOKABLE virtual void openMedia(bool append = false) = 0;

signals:
    void openSelected(const QList<QUrl> &urls, bool append);
    void failed(const QString &message);
};
