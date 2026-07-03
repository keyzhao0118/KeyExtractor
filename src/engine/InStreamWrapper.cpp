#include "InStreamWrapper.h"

#include <objidl.h>

InStreamWrapper::InStreamWrapper(const QString &filePath)
{
    m_file.setFileName(filePath);
    (void)m_file.open(QIODevice::ReadOnly);
}

InStreamWrapper::~InStreamWrapper()
{
    m_file.close();
}

bool InStreamWrapper::isOpen() const
{
    return m_file.isOpen();
}

STDMETHODIMP InStreamWrapper::Read(void *data, UInt32 size, UInt32 *processedSize)
{
    const qint64 bytesRead = m_file.read(static_cast<char *>(data), size);
    if (processedSize) {
        *processedSize = bytesRead > 0 ? static_cast<UInt32>(bytesRead) : 0;
    }

    return bytesRead < 0 ? E_FAIL : S_OK;
}

STDMETHODIMP InStreamWrapper::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
{
    qint64 targetPosition = 0;
    switch (seekOrigin) {
    case STREAM_SEEK_SET:
        targetPosition = offset;
        break;
    case STREAM_SEEK_CUR:
        targetPosition = m_file.pos() + offset;
        break;
    case STREAM_SEEK_END:
        targetPosition = m_file.size() + offset;
        break;
    default:
        return STG_E_INVALIDFUNCTION;
    }

    if (targetPosition < 0 || !m_file.seek(targetPosition)) {
        return E_FAIL;
    }

    if (newPosition) {
        *newPosition = static_cast<UInt64>(m_file.pos());
    }
    return S_OK;
}
