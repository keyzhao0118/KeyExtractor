#pragma once

#include <QString>
#include <QWidget>

class ArchiveListWorker;
class FileTreeModel;
class QLabel;
class QPushButton;
class QThread;
class QTreeView;

class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(const QString &archivePath = {}, QWidget *parent = nullptr);
    ~MainWindow() override;

    QString archivePath() const;

private:
    void setupUi();
    void setArchivePath(const QString &archivePath);
    void startArchiveLoad();
    void stopArchiveLoad();
    void updateWindowTitle();
    void updateStatusText();
    void requestExtractAll();

    QString m_archivePath;
    QTreeView *m_fileTreeView = nullptr;
    FileTreeModel *m_fileModel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_extractAllButton = nullptr;
    QThread *m_archiveThread = nullptr;
    ArchiveListWorker *m_archiveWorker = nullptr;
    bool m_isLoadingArchive = false;
    int m_loadedItemCount = 0;
};
