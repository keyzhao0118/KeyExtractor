#include "ArchiveListWorker.h"

#include "engine/SevenZipEngine.h"

ArchiveListWorker::ArchiveListWorker(QString archivePath, QObject *parent)
    : QObject(parent)
    , m_archivePath(std::move(archivePath))
{
}

void ArchiveListWorker::run()
{
    m_cancelled = false;

    SevenZipEngine engine;
    const SevenZipEngine::ListResult result = engine.listArchive(
        m_archivePath,
        [this](const ArchiveItem &item) {
            if (m_cancelled) {
                return false;
            }
            emit itemRead(item);
            return true;
        },
        [this](int count) {
            if (m_cancelled) {
                return false;
            }
            emit countChanged(count);
            return true;
        });

    emit finished(result.success, result.error, result.count);
}

void ArchiveListWorker::cancel()
{
    m_cancelled = true;
}
