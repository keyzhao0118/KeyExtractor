#include "SevenZipEngine.h"

#include "engine/InStreamWrapper.h"

#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>

#include <Common/MyCom.h>
#include <7zip/Archive/IArchive.h>
#include <7zip/PropID.h>
#include <windows.h>

namespace {

extern "C" const GUID CLSID_CFormatZip;
extern "C" const GUID CLSID_CFormat7z;
extern "C" const GUID CLSID_CFormatRar;
extern "C" const GUID CLSID_CFormatRar5;
extern "C" const GUID CLSID_CFormatTar;

typedef UINT32(WINAPI *CreateObjectFunc)(const GUID *clsID, const GUID *iid, void **outObject);
CreateObjectFunc resolveCreateObject(QString *error)
{
    static QLibrary sevenZipLib(QStringLiteral("7zip.dll"));
    static QMutex mutex;
    static CreateObjectFunc createObject = nullptr;

    QMutexLocker locker(&mutex);
    if (createObject) {
        return createObject;
    }

    sevenZipLib.setLoadHints(QLibrary::PreventUnloadHint);
    if (!sevenZipLib.load()) {
        if (error) {
            *error = QObject::tr("7zip.dll was not found or could not be loaded: %1").arg(sevenZipLib.errorString());
        }
        return nullptr;
    }

    createObject = reinterpret_cast<CreateObjectFunc>(sevenZipLib.resolve("CreateObject"));
    if (!createObject && error) {
        *error = QObject::tr("7zip.dll does not export CreateObject.");
    }
    return createObject;
}

bool isZip(const uint8_t *data)
{
    return data[0] == 0x50 && data[1] == 0x4B
        && (data[2] == 0x03 || data[2] == 0x05 || data[2] == 0x07)
        && (data[3] == 0x04 || data[3] == 0x06 || data[3] == 0x08);
}

bool is7z(const uint8_t *data)
{
    static constexpr uint8_t signature[] = {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C};
    return memcmp(data, signature, sizeof(signature)) == 0;
}

bool isRar4(const uint8_t *data)
{
    static constexpr uint8_t signature[] = {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00};
    return memcmp(data, signature, sizeof(signature)) == 0;
}

bool isRar5(const uint8_t *data)
{
    static constexpr uint8_t signature[] = {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00};
    return memcmp(data, signature, sizeof(signature)) == 0;
}

GUID clsidForArchive(const QString &archivePath)
{
    QFile file(archivePath);
    if (file.open(QIODevice::ReadOnly)) {
        uint8_t header[32] = {};
        const qint64 readSize = file.read(reinterpret_cast<char *>(header), sizeof(header));
        file.close();

        if (readSize >= 8) {
            if (is7z(header)) {
                return CLSID_CFormat7z;
            }
            if (isRar5(header)) {
                return CLSID_CFormatRar5;
            }
            if (isRar4(header)) {
                return CLSID_CFormatRar;
            }
            if (isZip(header)) {
                return CLSID_CFormatZip;
            }
        }
    }

    if (QFileInfo(archivePath).suffix().compare(QStringLiteral("tar"), Qt::CaseInsensitive) == 0) {
        return CLSID_CFormatTar;
    }

    return CLSID_NULL;
}

QString normalizedArchivePath(QString value)
{
    return value.replace(QLatin1Char('\\'), QLatin1Char('/'));
}

QString itemNameFromPath(const QString &path)
{
    const QString normalized = normalizedArchivePath(path);
    const QString trimmed = normalized.endsWith(QLatin1Char('/')) ? normalized.left(normalized.size() - 1) : normalized;
    const int slash = trimmed.lastIndexOf(QLatin1Char('/'));
    return slash >= 0 ? trimmed.mid(slash + 1) : trimmed;
}

QString fileTypeForPath(const QString &path, bool isDirectory)
{
    if (isDirectory) {
        return QObject::tr("Folder");
    }

    const QString suffix = QFileInfo(path).suffix();
    return suffix.isEmpty() ? QObject::tr("File") : suffix.toUpper() + QObject::tr(" File");
}

quint64 uint64FromProp(const PROPVARIANT &prop)
{
    switch (prop.vt) {
    case VT_UI8:
        return prop.uhVal.QuadPart;
    case VT_UI4:
        return prop.ulVal;
    case VT_UI2:
        return prop.uiVal;
    case VT_UI1:
        return prop.bVal;
    case VT_I8:
        return prop.hVal.QuadPart >= 0 ? static_cast<quint64>(prop.hVal.QuadPart) : 0;
    case VT_I4:
        return prop.lVal >= 0 ? static_cast<quint64>(prop.lVal) : 0;
    default:
        return 0;
    }
}

bool boolFromProp(const PROPVARIANT &prop)
{
    if (prop.vt == VT_BOOL) {
        return prop.boolVal != VARIANT_FALSE;
    }
    return false;
}

QString stringFromProp(const PROPVARIANT &prop)
{
    if (prop.vt == VT_BSTR && prop.bstrVal) {
        return QString::fromWCharArray(prop.bstrVal);
    }
    return {};
}

} // namespace

SevenZipEngine::ListResult SevenZipEngine::listArchive(const QString &archivePath, const ItemCallback &onItem, const CountCallback &onCount)
{
    ListResult result;

    if (!QFileInfo::exists(archivePath)) {
        result.error = QObject::tr("Archive does not exist: %1").arg(archivePath);
        return result;
    }

    QString libraryError;
    const CreateObjectFunc createObject = resolveCreateObject(&libraryError);
    if (!createObject) {
        result.error = libraryError;
        return result;
    }

    const GUID clsid = clsidForArchive(archivePath);
    if (clsid == CLSID_NULL) {
        result.error = QObject::tr("Unsupported or unrecognized archive format: %1").arg(archivePath);
        return result;
    }

    CMyComPtr<IInArchive> archive;
    if (createObject(&clsid, &IID_IInArchive, reinterpret_cast<void **>(&archive)) != S_OK) {
        result.error = QObject::tr("Failed to create 7zip archive reader.");
        return result;
    }

    auto *inStreamSpec = new InStreamWrapper(archivePath);
    CMyComPtr<IInStream> inStream(inStreamSpec);
    if (!inStreamSpec->isOpen()) {
        result.error = QObject::tr("Failed to open archive file: %1").arg(archivePath);
        return result;
    }

    const HRESULT openResult = archive->Open(inStream, nullptr, nullptr);
    if (openResult != S_OK) {
        result.error = QObject::tr("7zip failed to open archive. HRESULT=0x%1")
            .arg(static_cast<qulonglong>(openResult), 0, 16);
        return result;
    }

    UInt32 itemCount = 0;
    const HRESULT countResult = archive->GetNumberOfItems(&itemCount);
    if (countResult != S_OK) {
        result.error = QObject::tr("Failed to read archive item count. HRESULT=0x%1")
            .arg(static_cast<qulonglong>(countResult), 0, 16);
        return result;
    }

    int emittedCount = 0;
    for (UInt32 index = 0; index < itemCount; ++index) {
        PROPVARIANT propPath;
        PROPVARIANT propIsDir;
        PROPVARIANT propSize;
        PropVariantInit(&propPath);
        PropVariantInit(&propIsDir);
        PropVariantInit(&propSize);

        archive->GetProperty(index, kpidPath, &propPath);
        archive->GetProperty(index, kpidIsDir, &propIsDir);
        archive->GetProperty(index, kpidSize, &propSize);

        const QString path = normalizedArchivePath(stringFromProp(propPath));
        const bool isDirectory = boolFromProp(propIsDir) || path.endsWith(QLatin1Char('/'));
        const quint64 size = uint64FromProp(propSize);

        PropVariantClear(&propPath);
        PropVariantClear(&propIsDir);
        PropVariantClear(&propSize);

        if (path.isEmpty()) {
            continue;
        }

        ArchiveItem item;
        item.path = path;
        item.name = itemNameFromPath(path);
        item.isDirectory = isDirectory;
        item.size = size;
        item.type = fileTypeForPath(path, isDirectory);

        if (item.name.isEmpty()) {
            item.name = item.path;
        }

        if (onItem && !onItem(item)) {
            archive->Close();
            result.error = QObject::tr("Archive listing was cancelled.");
            return result;
        }

        ++emittedCount;
        if (onCount && !onCount(emittedCount)) {
            archive->Close();
            result.error = QObject::tr("Archive listing was cancelled.");
            return result;
        }
    }

    archive->Close();
    result.success = true;
    result.count = emittedCount;
    return result;
}
