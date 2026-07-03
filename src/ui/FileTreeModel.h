#pragma once

#include "core/ArchiveItem.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QString>
#include <vector>
#include <memory>

class FileTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit FileTreeModel(QObject *parent = nullptr);
    ~FileTreeModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void clear();
    void addArchiveItem(const ArchiveItem &item);

    int fileCount() const;
    int folderCount() const;
    quint64 totalSize() const;

private:
    struct Node {
        explicit Node(QString nodeName, Node *nodeParent = nullptr);

        QString name;
        QString type;
        QString fullPath;
        quint64 size = 0;
        bool isDirectory = false;
        Node *parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
    };

    Node *nodeFromIndex(const QModelIndex &index) const;
    Node *findChild(Node *parent, const QString &name) const;
    Node *ensureDirectory(Node *parent, const QString &name, const QString &fullPath);
    static QString formatSize(quint64 size);

    std::unique_ptr<Node> m_root;
    int m_fileCount = 0;
    int m_folderCount = 0;
    quint64 m_totalSize = 0;
};
