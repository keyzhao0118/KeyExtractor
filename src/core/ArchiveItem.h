#pragma once

#include <QMetaType>
#include <QString>
#include <QtGlobal>

struct ArchiveItem {
    QString path;
    QString name;
    QString type;
    quint64 size = 0;
    bool isDirectory = false;
};

Q_DECLARE_METATYPE(ArchiveItem)
