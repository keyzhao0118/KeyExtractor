#pragma once

#include "core/ArchiveItem.h"

#include <QString>
#include <functional>

class SevenZipEngine {
public:
    using ItemCallback = std::function<bool(const ArchiveItem &)>;
    using CountCallback = std::function<bool(int)>;

    struct ListResult {
        bool success = false;
        QString error;
        int count = 0;
    };

    ListResult listArchive(const QString &archivePath, const ItemCallback &onItem, const CountCallback &onCount);

};
