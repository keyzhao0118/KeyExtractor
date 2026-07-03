#include "FileTreeModel.h"

#include <algorithm>
#include <QBrush>
#include <QLocale>

namespace {

QString normalizedPath(QString value)
{
    value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (value.startsWith(QLatin1Char('/'))) {
        value.remove(0, 1);
    }
    while (value.endsWith(QLatin1Char('/'))) {
        value.chop(1);
    }
    return value;
}

} // namespace

FileTreeModel::Node::Node(QString nodeName, Node *nodeParent)
    : name(std::move(nodeName))
    , parent(nodeParent)
{
}

FileTreeModel::FileTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_root(std::make_unique<Node>(QString()))
{
    m_root->isDirectory = true;
}

FileTreeModel::~FileTreeModel() = default;

QModelIndex FileTreeModel::index(int row, int column, const QModelIndex &parentIndex) const
{
    if (!hasIndex(row, column, parentIndex)) {
        return {};
    }

    Node *parentNode = nodeFromIndex(parentIndex);
    if (!parentNode || row < 0 || row >= parentNode->children.size()) {
        return {};
    }

    return createIndex(row, column, parentNode->children.at(row).get());
}

QModelIndex FileTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return {};
    }

    auto *childNode = static_cast<Node *>(child.internalPointer());
    if (!childNode || !childNode->parent || childNode->parent == m_root.get()) {
        return {};
    }

    Node *parentNode = childNode->parent;
    Node *grandParent = parentNode->parent;
    if (!grandParent) {
        return {};
    }

    for (int row = 0; row < grandParent->children.size(); ++row) {
        if (grandParent->children.at(row).get() == parentNode) {
            return createIndex(row, 0, parentNode);
        }
    }

    return {};
}

int FileTreeModel::rowCount(const QModelIndex &parentIndex) const
{
    if (parentIndex.isValid() && parentIndex.column() != 0) {
        return 0;
    }

    const Node *parentNode = nodeFromIndex(parentIndex);
    return parentNode ? static_cast<int>(parentNode->children.size()) : 0;
}

int FileTreeModel::columnCount(const QModelIndex &) const
{
    return 3;
}

QVariant FileTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const auto *node = static_cast<const Node *>(index.internalPointer());
    if (!node) {
        return {};
    }

    if (role == Qt::TextAlignmentRole && index.column() == 2) {
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }

    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
    case 0:
        return node->name;
    case 1:
        return node->isDirectory ? tr("Folder") : node->type;
    case 2:
        return node->isDirectory ? QString() : formatSize(node->size);
    default:
        return {};
    }
}

QVariant FileTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
    case 0:
        return tr("Name");
    case 1:
        return tr("Type");
    case 2:
        return tr("Extracted Size");
    default:
        return {};
    }
}

void FileTreeModel::clear()
{
    beginResetModel();
    m_root = std::make_unique<Node>(QString());
    m_root->isDirectory = true;
    m_fileCount = 0;
    m_folderCount = 0;
    m_totalSize = 0;
    endResetModel();
}

void FileTreeModel::addArchiveItem(const ArchiveItem &item)
{
    const QString path = normalizedPath(item.path);
    if (path.isEmpty()) {
        return;
    }

    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return;
    }

    Node *parentNode = m_root.get();
    QString currentPath;

    for (int i = 0; i < parts.size() - 1; ++i) {
        currentPath += (currentPath.isEmpty() ? QString() : QStringLiteral("/")) + parts.at(i);
        parentNode = ensureDirectory(parentNode, parts.at(i), currentPath);
    }

    const QString itemName = item.name.isEmpty() ? parts.last() : item.name;
    Node *existing = findChild(parentNode, itemName);
    if (existing) {
        existing->isDirectory = item.isDirectory;
        existing->type = item.type;
        existing->size = item.size;
        const int changedRow = static_cast<int>(std::distance(parentNode->children.begin(),
            std::find_if(parentNode->children.begin(), parentNode->children.end(), [existing](const auto &child) {
                return child.get() == existing;
            })));
        const QModelIndex changed = createIndex(changedRow, 0, existing);
        emit dataChanged(changed, changed.siblingAtColumn(2));
        return;
    }

    const QModelIndex parentIndex = parentNode == m_root.get() ? QModelIndex() : createIndex(
        parentNode->parent ? static_cast<int>(std::distance(parentNode->parent->children.begin(),
            std::find_if(parentNode->parent->children.begin(), parentNode->parent->children.end(), [parentNode](const auto &child) {
                return child.get() == parentNode;
            }))) : 0,
        0,
        parentNode);
    const int row = static_cast<int>(parentNode->children.size());
    beginInsertRows(parentIndex, row, row);
    auto node = std::make_unique<Node>(itemName, parentNode);
    node->fullPath = path;
    node->isDirectory = item.isDirectory;
    node->type = item.type;
    node->size = item.size;
    parentNode->children.push_back(std::move(node));
    endInsertRows();

    if (item.isDirectory) {
        ++m_folderCount;
    } else {
        ++m_fileCount;
        m_totalSize += item.size;
    }
}

int FileTreeModel::fileCount() const
{
    return m_fileCount;
}

int FileTreeModel::folderCount() const
{
    return m_folderCount;
}

quint64 FileTreeModel::totalSize() const
{
    return m_totalSize;
}

FileTreeModel::Node *FileTreeModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return m_root.get();
    }

    return static_cast<Node *>(index.internalPointer());
}

FileTreeModel::Node *FileTreeModel::findChild(Node *parent, const QString &name) const
{
    if (!parent) {
        return nullptr;
    }

    for (const auto &child : parent->children) {
        if (child->name == name) {
            return child.get();
        }
    }

    return nullptr;
}

FileTreeModel::Node *FileTreeModel::ensureDirectory(Node *parent, const QString &name, const QString &fullPath)
{
    if (Node *existing = findChild(parent, name)) {
        return existing;
    }

    const QModelIndex parentIndex = parent == m_root.get() ? QModelIndex() : createIndex(
        parent->parent ? static_cast<int>(std::distance(parent->parent->children.begin(),
            std::find_if(parent->parent->children.begin(), parent->parent->children.end(), [parent](const auto &child) {
                return child.get() == parent;
            }))) : 0,
        0,
        parent);

    const int row = static_cast<int>(parent->children.size());
    beginInsertRows(parentIndex, row, row);
    auto node = std::make_unique<Node>(name, parent);
    node->fullPath = fullPath;
    node->isDirectory = true;
    node->type = tr("Folder");
    Node *rawNode = node.get();
    parent->children.push_back(std::move(node));
    endInsertRows();

    ++m_folderCount;
    return rawNode;
}

QString FileTreeModel::formatSize(quint64 size)
{
    return QLocale().formattedDataSize(size);
}
