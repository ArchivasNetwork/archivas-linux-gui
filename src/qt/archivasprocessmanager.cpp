#include "archivasprocessmanager.h"
#include <QDebug>
#include <QFileInfo>

ArchivasProcessManager::ArchivasProcessManager(QObject *parent)
    : QObject(parent)
    , m_nodeProcess(nullptr)
    , m_farmerProcess(nullptr)
{
    m_nodeProcess = new QProcess(this);
    m_farmerProcess = new QProcess(this);
    setupNodeProcess();
    setupFarmerProcess();
}

ArchivasProcessManager::~ArchivasProcessManager()
{
    stopNode();
    stopFarmer();
}

void ArchivasProcessManager::setupNodeProcess()
{
    // Set process channel mode to merge stderr into stdout first
    m_nodeProcess->setProcessChannelMode(QProcess::MergedChannels);
    
    // With MergedChannels, we only need to connect to readyRead (not separate stdout/stderr)
    connect(m_nodeProcess, &QProcess::readyRead, 
            this, &ArchivasProcessManager::onNodeReadyRead, Qt::QueuedConnection);
    connect(m_nodeProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ArchivasProcessManager::onNodeFinished, Qt::QueuedConnection);
    connect(m_nodeProcess, &QProcess::errorOccurred, 
            this, &ArchivasProcessManager::onNodeError, Qt::QueuedConnection);
}

void ArchivasProcessManager::setupFarmerProcess()
{
    // Set process channel mode to merge stderr into stdout first
    m_farmerProcess->setProcessChannelMode(QProcess::MergedChannels);
    
    // With MergedChannels, we only need to connect to readyRead (not separate stdout/stderr)
    connect(m_farmerProcess, &QProcess::readyRead, 
            this, &ArchivasProcessManager::onFarmerReadyRead, Qt::QueuedConnection);
    connect(m_farmerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ArchivasProcessManager::onFarmerFinished, Qt::QueuedConnection);
    connect(m_farmerProcess, &QProcess::errorOccurred, 
            this, &ArchivasProcessManager::onFarmerError, Qt::QueuedConnection);
}

bool ArchivasProcessManager::startNode(const QString& executablePath, const QString& network,
                                       const QString& rpcBind, const QString& dataDir,
                                       const QString& bootnodes)
{
    if (isNodeRunning()) {
        qWarning() << "Node is already running";
        return false;
    }

    QFileInfo fileInfo(executablePath);
    if (!fileInfo.exists() || !fileInfo.isExecutable()) {
        emit nodeError(QString("Node executable not found or not executable: %1").arg(executablePath));
        return false;
    }

    m_nodeExecutablePath = executablePath;
    QStringList args = buildNodeArgs(network, rpcBind, dataDir, bootnodes);

    m_nodeProcess->setProgram(executablePath);
    m_nodeProcess->setArguments(args);
    // Channel mode is already set in setupNodeProcess()
    m_nodeProcess->start();

    if (m_nodeProcess->waitForStarted(5000)) {
        emit nodeStarted();
        return true;
    } else {
        QString error = m_nodeProcess->errorString();
        emit nodeError(error);
        return false;
    }
}

void ArchivasProcessManager::stopNode()
{
    if (!isNodeRunning()) {
        return;
    }

    m_nodeProcess->terminate();
    if (!m_nodeProcess->waitForFinished(5000)) {
        m_nodeProcess->kill();
        m_nodeProcess->waitForFinished(1000);
    }
    emit nodeStopped();
}

bool ArchivasProcessManager::isNodeRunning() const
{
    return m_nodeProcess->state() == QProcess::Running;
}

bool ArchivasProcessManager::startFarmer(const QString& executablePath, const QString& plotsPath,
                                         const QString& farmerPrivkeyPath, const QString& nodeUrl)
{
    if (isFarmerRunning()) {
        qWarning() << "Farmer is already running";
        return false;
    }

    QFileInfo fileInfo(executablePath);
    if (!fileInfo.exists() || !fileInfo.isExecutable()) {
        emit farmerError(QString("Farmer executable not found or not executable: %1").arg(executablePath));
        return false;
    }

    m_farmerExecutablePath = executablePath;
    QStringList args = buildFarmerArgs(plotsPath, farmerPrivkeyPath, nodeUrl);

    m_farmerProcess->setProgram(executablePath);
    m_farmerProcess->setArguments(args);
    // Channel mode is already set in setupFarmerProcess()
    m_farmerProcess->start();

    if (m_farmerProcess->waitForStarted(5000)) {
        emit farmerStarted();
        return true;
    } else {
        QString error = m_farmerProcess->errorString();
        emit farmerError(error);
        return false;
    }
}

void ArchivasProcessManager::stopFarmer()
{
    if (!isFarmerRunning()) {
        return;
    }

    m_farmerProcess->terminate();
    if (!m_farmerProcess->waitForFinished(5000)) {
        m_farmerProcess->kill();
        m_farmerProcess->waitForFinished(1000);
    }
    emit farmerStopped();
}

bool ArchivasProcessManager::isFarmerRunning() const
{
    return m_farmerProcess->state() == QProcess::Running;
}

QStringList ArchivasProcessManager::buildNodeArgs(const QString& network, const QString& rpcBind,
                                                   const QString& dataDir, const QString& bootnodes)
{
    QStringList args;
    args << "--network-id" << network;
    args << "--rpc" << rpcBind;
    args << "--data-dir" << dataDir;
    if (!bootnodes.isEmpty()) {
        args << "--bootnodes" << bootnodes;
    }
    return args;
}

QStringList ArchivasProcessManager::buildFarmerArgs(const QString& plotsPath,
                                                     const QString& farmerPrivkeyPath,
                                                     const QString& nodeUrl)
{
    QStringList args;
    args << "farm";
    args << "--node" << nodeUrl;
    args << "--plots" << plotsPath;
    args << "--farmer-privkey" << farmerPrivkeyPath;
    return args;
}

void ArchivasProcessManager::onNodeReadyRead()
{
    if (!m_nodeProcess) {
        return;
    }
    
    // Read all available data
    QByteArray data = m_nodeProcess->readAll();
    
    if (data.isEmpty()) {
        return;
    }

    QString output = QString::fromUtf8(data);
    m_nodeOutput += output;

    // Filter out Qt warnings that clutter the logs
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        // Filter out QSocketNotifier warnings
        if (trimmed.contains("QSocketNotifier") || trimmed.contains("Can only be used with threads")) {
            continue;
        }
        if (!trimmed.isEmpty()) {
            emit nodeOutput(trimmed);
        }
    }
}

void ArchivasProcessManager::onNodeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    if (exitStatus == QProcess::NormalExit) {
        emit nodeStopped();
    } else {
        emit nodeError("Node process crashed");
        emit nodeStopped();
    }
}

void ArchivasProcessManager::onNodeError(QProcess::ProcessError error)
{
    QString errorMsg;
    QString details;
    
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "Node failed to start";
        if (m_nodeProcess) {
            details = QString("Error: %1").arg(m_nodeProcess->errorString());
            if (m_nodeProcess->errorString().contains("No such file")) {
                details += " - The executable file was not found. Check the path.";
            } else if (m_nodeProcess->errorString().contains("Permission denied")) {
                details += " - The file is not executable. Check file permissions.";
            }
        }
        break;
    case QProcess::Crashed:
        errorMsg = "Node process crashed";
        if (m_nodeProcess) {
            details = QString("Exit code: %1").arg(m_nodeProcess->exitCode());
        }
        break;
    case QProcess::Timedout:
        errorMsg = "Node process timed out";
        break;
    case QProcess::WriteError:
        errorMsg = "Node write error";
        break;
    case QProcess::ReadError:
        errorMsg = "Node read error";
        break;
    default:
        errorMsg = "Unknown node error";
        break;
    }
    
    if (!details.isEmpty()) {
        errorMsg += " - " + details;
    }
    
    emit nodeError(errorMsg);
}

void ArchivasProcessManager::onFarmerReadyRead()
{
    if (!m_farmerProcess) {
        return;
    }
    
    // Read all available data
    QByteArray data = m_farmerProcess->readAll();
    
    if (data.isEmpty()) {
        return;
    }

    QString output = QString::fromUtf8(data);
    m_farmerOutput += output;

    // Filter out Qt warnings that clutter the logs
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        // Filter out QSocketNotifier warnings
        if (trimmed.contains("QSocketNotifier") || trimmed.contains("Can only be used with threads")) {
            continue;
        }
        if (!trimmed.isEmpty()) {
            emit farmerOutput(trimmed);
        }
    }
}

void ArchivasProcessManager::onFarmerFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    if (exitStatus == QProcess::NormalExit) {
        emit farmerStopped();
    } else {
        emit farmerError("Farmer process crashed");
        emit farmerStopped();
    }
}

void ArchivasProcessManager::onFarmerError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "Farmer failed to start";
        break;
    case QProcess::Crashed:
        errorMsg = "Farmer process crashed";
        break;
    case QProcess::Timedout:
        errorMsg = "Farmer process timed out";
        break;
    case QProcess::WriteError:
        errorMsg = "Farmer write error";
        break;
    case QProcess::ReadError:
        errorMsg = "Farmer read error";
        break;
    default:
        errorMsg = "Unknown farmer error";
        break;
    }
    emit farmerError(errorMsg);
}

