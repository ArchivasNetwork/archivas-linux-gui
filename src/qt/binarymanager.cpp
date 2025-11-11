#include "binarymanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDebug>

BinaryManager* BinaryManager::instance()
{
    static BinaryManager* inst = nullptr;
    if (!inst) {
        inst = new BinaryManager();
    }
    return inst;
}

BinaryManager::BinaryManager()
{
    // Determine where to extract binaries
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QDir::homePath() + "/.archivas-core";
    }
    m_binariesDir = dataDir + "/binaries";
    
    // Ensure directory exists
    QDir dir;
    dir.mkpath(m_binariesDir);
}

BinaryManager::~BinaryManager()
{
}

QString BinaryManager::getNodeBinaryPath()
{
    QString nodePath = m_binariesDir + "/archivas-node";
    
    // Check if binary already exists
    if (QFile::exists(nodePath) && QFileInfo(nodePath).isExecutable()) {
        return nodePath;
    }
    
    // Extract from resources
    if (extractBinary(":/binaries/archivas-node", nodePath)) {
        // Make executable
        QFile::setPermissions(nodePath, QFile::ReadUser | QFile::WriteUser | QFile::ExeUser | 
                                             QFile::ReadGroup | QFile::ExeGroup |
                                             QFile::ReadOther | QFile::ExeOther);
        return nodePath;
    }
    
    return "";
}

QString BinaryManager::getFarmerBinaryPath()
{
    QString farmerPath = m_binariesDir + "/archivas-farmer";
    
    // Check if binary already exists
    if (QFile::exists(farmerPath) && QFileInfo(farmerPath).isExecutable()) {
        return farmerPath;
    }
    
    // Extract from resources
    if (extractBinary(":/binaries/archivas-farmer", farmerPath)) {
        // Make executable
        QFile::setPermissions(farmerPath, QFile::ReadUser | QFile::WriteUser | QFile::ExeUser | 
                                              QFile::ReadGroup | QFile::ExeGroup |
                                              QFile::ReadOther | QFile::ExeOther);
        return farmerPath;
    }
    
    return "";
}

bool BinaryManager::extractBinary(const QString& resourcePath, const QString& targetPath)
{
    // Check if resource exists
    if (!QFile::exists(resourcePath)) {
        qWarning() << "Binary resource not found:" << resourcePath;
        return false;
    }
    
    // Remove existing file if it exists
    if (QFile::exists(targetPath)) {
        QFile::remove(targetPath);
    }
    
    // Copy from resource to target
    QFile resourceFile(resourcePath);
    if (!resourceFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open resource:" << resourcePath;
        return false;
    }
    
    QFile targetFile(targetPath);
    if (!targetFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to create target file:" << targetPath;
        resourceFile.close();
        return false;
    }
    
    // Copy data
    QByteArray data = resourceFile.readAll();
    if (targetFile.write(data) != data.size()) {
        qWarning() << "Failed to write binary data to:" << targetPath;
        resourceFile.close();
        targetFile.close();
        QFile::remove(targetPath);
        return false;
    }
    
    resourceFile.close();
    targetFile.close();
    
    qDebug() << "Extracted binary:" << targetPath;
    return true;
}

QString BinaryManager::getBinariesDirectory() const
{
    return m_binariesDir;
}

bool BinaryManager::areBinariesAvailable()
{
    QString nodePath = getNodeBinaryPath();
    QString farmerPath = getFarmerBinaryPath();
    
    return !nodePath.isEmpty() && !farmerPath.isEmpty() &&
           QFile::exists(nodePath) && QFile::exists(farmerPath) &&
           QFileInfo(nodePath).isExecutable() && QFileInfo(farmerPath).isExecutable();
}

