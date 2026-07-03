#pragma once

#include <QFile>

#include <Common/MyCom.h>
#include <7zip/IStream.h>

class InStreamWrapper : public IInStream, public CMyUnknownImp {
    Z7_COM_UNKNOWN_IMP_1(IInStream)

public:
    explicit InStreamWrapper(const QString &filePath);
    ~InStreamWrapper();

    bool isOpen() const;

    STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize) override;
    STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition) override;

private:
    QFile m_file;
};
