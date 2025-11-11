#include "archivasapplication.h"
#include "mainwindow.h"
#include <QLockFile>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QDateTime>
#include <QProcess>

int main(int argc, char *argv[])
{
    // Suppress Qt debug output and warnings
    QLoggingCategory::setFilterRules(
        "qt.core.qobject.warning=false\n"
        "*.debug=false\n"
        "qt.*=false"
    );
    
    // Suppress Qt plugin loading messages
    qputenv("QT_LOGGING_RULES", "*.debug=false;qt.*=false");
    
    ArchivasApplication app(argc, argv);

    // Ensure only one instance is running using lock file
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QLockFile lockFile(tempPath + "/archivas-core-gui.lock");
    
    // Remove stale lock files (older than 1 hour)
    if (lockFile.isLocked()) {
        QFileInfo lockInfo(tempPath + "/archivas-core-gui.lock");
        if (lockInfo.exists() && lockInfo.lastModified().secsTo(QDateTime::currentDateTime()) > 3600) {
            lockFile.removeStaleLockFile();
        }
    }
    
    if (!lockFile.tryLock(100)) {
        // Check if another instance is actually running
        QProcess checkProcess;
        checkProcess.start("pgrep", QStringList() << "-f" << "archivas-qt");
        checkProcess.waitForFinished(1000);
        if (checkProcess.exitCode() == 0) {
            // Another instance is running, exit silently
            return 1;
        } else {
            // No other instance, remove stale lock and try again
            lockFile.removeStaleLockFile();
            if (!lockFile.tryLock(100)) {
                return 1;
            }
        }
    }

    // Set application properties
    app.setQuitOnLastWindowClosed(true);
    app.setOrganizationName("Archivas Network");
    app.setApplicationName("Archivas Core");

    // Create and show main window (only one window)
    MainWindow window;
    window.show();

    return app.exec();
}

