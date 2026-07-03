#pragma once

#include "core/ArchiveItem.h"

#include <atomic>

#include <QObject>
#include <QString>

class ArchiveListWorker : public QObject {
    Q_OBJECT

public:
    explicit ArchiveListWorker(QString archivePath, QObject *parent = nullptr);

public slots:
    void run();
    void cancel();

signals:
    void itemRead(const ArchiveItem &item);
    void countChanged(int count);
    void finished(bool success, const QString &errorMessage, int totalCount);

private:
    QString m_archivePath;
    std::atomic_bool m_cancelled = false;
};
