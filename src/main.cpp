#include "ui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QStringList>
#include <QTranslator>
#include <QWidget>

namespace {

constexpr int kMaxArchivesPerLaunch = 10;

enum class LaunchMode {
    Open,
    Extract,
};

struct LaunchRequest {
    LaunchMode mode = LaunchMode::Open;
    QStringList archivePaths;
    QString error;
    QStringList warnings;
};

QString stripOuterQuotes(QString value)
{
    if (value.size() >= 2 && value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"')) {
        value = value.mid(1, value.size() - 2);
    }

    return value;
}

void installQtTranslations(QCoreApplication &app)
{
    const QLocale locale;
    const QStringList translationDirs = {
        QLibraryInfo::path(QLibraryInfo::TranslationsPath),
        QCoreApplication::applicationDirPath() + QLatin1String("/translations"),
    };

    static QTranslator qtBaseTranslator;
    static QTranslator qtTranslator;

    for (const QString &translationDir : translationDirs) {
        if (translationDir.isEmpty() || !QDir(translationDir).exists()) {
            continue;
        }

        if (qtBaseTranslator.isEmpty() && qtBaseTranslator.load(locale, QLatin1String("qtbase"), QLatin1String("_"), translationDir)) {
            app.installTranslator(&qtBaseTranslator);
        }

        if (qtTranslator.isEmpty() && qtTranslator.load(locale, QLatin1String("qt"), QLatin1String("_"), translationDir)) {
            app.installTranslator(&qtTranslator);
        }
    }
}

LaunchRequest parseLaunchRequest(const QStringList &arguments)
{
    LaunchRequest request;
    QStringList args = arguments;
    if (!args.isEmpty()) {
        args.removeFirst();
    }

    if (!args.isEmpty() && args.first() == QLatin1String("--extract")) {
        request.mode = LaunchMode::Extract;
        args.removeFirst();
    } else if (!args.isEmpty() && args.first().startsWith(QLatin1String("--"))) {
        request.error = QObject::tr("Unknown command line option: %1").arg(args.first());
        return request;
    }

    if (request.mode == LaunchMode::Extract && args.isEmpty()) {
        request.error = QObject::tr("Missing archive path after --extract.");
        return request;
    }

    if (args.size() > kMaxArchivesPerLaunch) {
        request.warnings.push_back(QObject::tr("Only the first %1 archives will be processed.").arg(kMaxArchivesPerLaunch));
        args = args.mid(0, kMaxArchivesPerLaunch);
    }

    for (const QString &arg : args) {
        const QString path = stripOuterQuotes(arg).trimmed();
        if (path.isEmpty()) {
            continue;
        }

        request.archivePaths.push_back(QFileInfo(path).absoluteFilePath());
    }

    return request;
}

QWidget *createMainWindow(const QString &archivePath = {})
{
    return new MainWindow(archivePath);
}

int runExtractFlow(QWidget *parent, const QStringList &archivePaths)
{
    const QString destinationDir = QFileDialog::getExistingDirectory(
        parent,
        QObject::tr("Select Extraction Folder"),
        archivePaths.isEmpty() ? QString() : QFileInfo(archivePaths.first()).absolutePath());
    if (destinationDir.isEmpty()) {
        return 0;
    }

    QMessageBox::information(
        parent,
        QObject::tr("Extraction Requested"),
        QObject::tr("Destination:\n%1\n\nArchives:\n%2\n\nArchiveManager::extractAll(destDir) will be called serially here once the core module is available.")
            .arg(destinationDir, archivePaths.join(QLatin1Char('\n'))));

    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    installQtTranslations(app);

    QApplication::setApplicationName(QStringLiteral("KeyExtractor"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setOrganizationName(QStringLiteral("KeyExtractor"));

    const LaunchRequest request = parseLaunchRequest(QCoreApplication::arguments());
    if (!request.error.isEmpty()) {
        QMessageBox::critical(nullptr, QObject::tr("KeyExtractor"), request.error);
        return 2;
    }

    for (const QString &warning : request.warnings) {
        QMessageBox::warning(nullptr, QObject::tr("KeyExtractor"), warning);
    }

    if (request.mode == LaunchMode::Extract) {
        return runExtractFlow(nullptr, request.archivePaths);
    }

    if (request.archivePaths.isEmpty()) {
        createMainWindow()->show();
    } else {
        for (const QString &archivePath : request.archivePaths) {
            createMainWindow(archivePath)->show();
        }
    }

    return app.exec();
}

