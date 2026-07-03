#include "MainWindow.h"

#include "core/ArchiveListWorker.h"
#include "ui/FileTreeModel.h"

#include <QAbstractItemView>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QThread>
#include <QTreeView>
#include <QVBoxLayout>

namespace {

constexpr int kDefaultWindowWidth = 720;
constexpr int kDefaultWindowHeight = 480;
constexpr int kMinimumWindowWidth = 560;
constexpr int kMinimumWindowHeight = 360;
constexpr int kContentMargin = 12;
constexpr int kExtractButtonWidth = 88;
constexpr int kExtractButtonHeight = 30;
constexpr int kTypeColumnWidth = 120;
constexpr int kSizeColumnWidth = 120;
constexpr int kTreeRowHeight = 28;

} // namespace

MainWindow::MainWindow(const QString &archivePath, QWidget *parent)
    : QWidget(parent)
{
    qRegisterMetaType<ArchiveItem>("ArchiveItem");
    setupUi();
    setArchivePath(archivePath);
}

MainWindow::~MainWindow()
{
    stopArchiveLoad();
}

QString MainWindow::archivePath() const
{
    return m_archivePath;
}

void MainWindow::setupUi()
{
    setAttribute(Qt::WA_DeleteOnClose);
    resize(kDefaultWindowWidth, kDefaultWindowHeight);
    setMinimumSize(kMinimumWindowWidth, kMinimumWindowHeight);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    mainLayout->setSpacing(kContentMargin);

    m_fileModel = new FileTreeModel(this);

    m_fileTreeView = new QTreeView(this);
    m_fileTreeView->setModel(m_fileModel);
    m_fileTreeView->setRootIsDecorated(true);
    m_fileTreeView->setAlternatingRowColors(true);
    m_fileTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileTreeView->setUniformRowHeights(true);
    m_fileTreeView->setIndentation(kTreeRowHeight);

    auto *header = m_fileTreeView->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::Fixed);
    header->setSectionResizeMode(2, QHeaderView::Fixed);
    header->resizeSection(1, kTypeColumnWidth);
    header->resizeSection(2, kSizeColumnWidth);
    m_fileTreeView->setColumnWidth(1, kTypeColumnWidth);
    m_fileTreeView->setColumnWidth(2, kSizeColumnWidth);

    auto *statusLayout = new QHBoxLayout;
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(kContentMargin);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_extractAllButton = new QPushButton(tr("Extract All"), this);
    m_extractAllButton->setFixedSize(kExtractButtonWidth, kExtractButtonHeight);
    connect(m_extractAllButton, &QPushButton::clicked, this, [this]() {
        requestExtractAll();
    });

    statusLayout->addWidget(m_statusLabel, 1);
    statusLayout->addWidget(m_extractAllButton, 0, Qt::AlignRight | Qt::AlignVCenter);

    mainLayout->addWidget(m_fileTreeView, 1);
    mainLayout->addLayout(statusLayout);
}

void MainWindow::setArchivePath(const QString &archivePath)
{
    stopArchiveLoad();
    m_archivePath = archivePath;
    m_fileModel->clear();
    m_loadedItemCount = 0;
    updateWindowTitle();
    updateStatusText();
    m_extractAllButton->setEnabled(false);

    if (!m_archivePath.isEmpty()) {
        startArchiveLoad();
    }
}

void MainWindow::startArchiveLoad()
{
    m_isLoadingArchive = true;
    m_loadedItemCount = 0;
    updateStatusText();
    m_extractAllButton->setEnabled(false);

    m_archiveThread = new QThread(this);
    m_archiveWorker = new ArchiveListWorker(m_archivePath);
    m_archiveWorker->moveToThread(m_archiveThread);

    connect(m_archiveThread, &QThread::started, m_archiveWorker, &ArchiveListWorker::run);
    connect(m_archiveWorker, &ArchiveListWorker::itemRead, this, [this](const ArchiveItem &item) {
        m_fileModel->addArchiveItem(item);
    });
    connect(m_archiveWorker, &ArchiveListWorker::countChanged, this, [this](int count) {
        m_loadedItemCount = count;
        updateStatusText();
    });
    connect(m_archiveWorker, &ArchiveListWorker::finished, this, [this](bool success, const QString &errorMessage, int totalCount) {
        m_isLoadingArchive = false;
        m_loadedItemCount = totalCount;
        updateStatusText();
        m_extractAllButton->setEnabled(success && !m_archivePath.isEmpty() && m_loadedItemCount > 0);

        if (!success && !errorMessage.isEmpty()) {
            QMessageBox::critical(this, tr("Open Archive Failed"), errorMessage);
        }

        if (m_archiveThread) {
            m_archiveThread->quit();
        }
    });
    connect(m_archiveWorker, &ArchiveListWorker::finished, m_archiveWorker, &QObject::deleteLater);
    connect(m_archiveThread, &QThread::finished, m_archiveThread, &QObject::deleteLater);
    connect(m_archiveThread, &QThread::finished, this, [this]() {
        m_archiveThread = nullptr;
        m_archiveWorker = nullptr;
    });

    m_archiveThread->start();
}

void MainWindow::stopArchiveLoad()
{
    if (!m_archiveThread) {
        m_archiveWorker = nullptr;
        m_isLoadingArchive = false;
        return;
    }

    if (m_archiveWorker) {
        m_archiveWorker->cancel();
    }

    m_archiveThread->quit();
    m_archiveThread->wait();
    m_archiveThread = nullptr;
    m_archiveWorker = nullptr;
    m_isLoadingArchive = false;
}

void MainWindow::updateWindowTitle()
{
    if (m_archivePath.isEmpty()) {
        setWindowTitle(QStringLiteral("KeyExtractor"));
        return;
    }

    setWindowTitle(QStringLiteral("KeyExtractor - %1").arg(QFileInfo(m_archivePath).fileName()));
}

void MainWindow::updateStatusText()
{
    if (m_archivePath.isEmpty()) {
        m_statusLabel->setText(tr("Open an archive to view its contents."));
        return;
    }

    if (m_isLoadingArchive) {
        m_statusLabel->setText(tr("Reading archive metadata... %1 items read").arg(m_loadedItemCount));
        return;
    }

    m_statusLabel->setText(tr("%1 files, %2 folders | extracted size %3")
        .arg(m_fileModel->fileCount())
        .arg(m_fileModel->folderCount())
        .arg(QLocale().formattedDataSize(m_fileModel->totalSize())));
}

void MainWindow::requestExtractAll()
{
    //To Do
}
