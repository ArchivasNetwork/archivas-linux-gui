#include "archivasnodemanager.h"
#include <QDebug>
#include <QByteArray>
#include <QMetaObject>

ArchivasNodeManager* ArchivasNodeManager::s_instance = nullptr;

ArchivasNodeManager::ArchivasNodeManager(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    
    // Set up log callbacks
    archivas_node_set_log_callback(logCallback);
    archivas_farmer_set_log_callback(farmerLogCallback);
    
    // Status update timer
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &ArchivasNodeManager::updateStatus);
    m_statusTimer->start(5000); // Update every 5 seconds
}

ArchivasNodeManager::~ArchivasNodeManager()
{
    stopNode();
    stopFarmer();
}

bool ArchivasNodeManager::startNode(const QString &networkId, const QString &rpcBind, 
                                     const QString &dataDir, const QString &bootnodes, const QString &genesisPath)
{
    QByteArray networkIdBytes = networkId.toUtf8();
    QByteArray rpcBindBytes = rpcBind.toUtf8();
    QByteArray dataDirBytes = dataDir.toUtf8();
    QByteArray bootnodesBytes = bootnodes.toUtf8();
    QByteArray genesisPathBytes = genesisPath.toUtf8();
    
    // Note: Go bridge expects char* not const char*, but we're passing const data
    // This is safe because the Go code only reads the strings
    int result = archivas_node_start(
        const_cast<char*>(networkIdBytes.constData()),
        const_cast<char*>(rpcBindBytes.constData()),
        const_cast<char*>(dataDirBytes.constData()),
        const_cast<char*>(bootnodesBytes.constData()),
        const_cast<char*>(genesisPathBytes.constData())
    );
    
    if (result == 0) {
        emit nodeStarted();
        return true;
    }
    return false;
}

void ArchivasNodeManager::stopNode()
{
    archivas_node_stop();
    emit nodeStopped();
}

bool ArchivasNodeManager::isNodeRunning() const
{
    return archivas_node_is_running() != 0;
}

bool ArchivasNodeManager::startFarmer(const QString &nodeUrl, const QString &plotsPath, 
                                       const QString &farmerPrivkey)
{
    QByteArray nodeUrlBytes = nodeUrl.toUtf8();
    QByteArray plotsPathBytes = plotsPath.toUtf8();
    QByteArray farmerPrivkeyBytes = farmerPrivkey.toUtf8();
    
    // Note: Go bridge expects char* not const char*, but we're passing const data
    // This is safe because the Go code only reads the strings
    int result = archivas_farmer_start(
        const_cast<char*>(nodeUrlBytes.constData()),
        const_cast<char*>(plotsPathBytes.constData()),
        const_cast<char*>(farmerPrivkeyBytes.constData())
    );
    
    if (result == 0) {
        emit farmerStarted();
        return true;
    }
    return false;
}

void ArchivasNodeManager::stopFarmer()
{
    archivas_farmer_stop();
    emit farmerStopped();
}

bool ArchivasNodeManager::isFarmerRunning() const
{
    return archivas_farmer_is_running() != 0;
}

int ArchivasNodeManager::getCurrentHeight() const
{
    return archivas_node_get_height();
}

QString ArchivasNodeManager::getTipHash() const
{
    char* hash = archivas_node_get_tip_hash();
    if (hash) {
        QString result = QString::fromUtf8(hash);
        // Note: We don't free the C string here because it's managed by Go
        // In a production implementation, we'd need to coordinate memory management
        return result;
    }
    return QString();
}

int ArchivasNodeManager::getPeerCount() const
{
    return archivas_node_get_peer_count();
}

int ArchivasNodeManager::getPlotCount() const
{
    return archivas_farmer_get_plot_count();
}

QString ArchivasNodeManager::getLastProof() const
{
    char* proof = archivas_farmer_get_last_proof();
    if (proof) {
        QString result = QString::fromUtf8(proof);
        // Note: We don't free the C string here because it's managed by Go
        return result;
    }
    return QString();
}

bool ArchivasNodeManager::createPlot(const QString &plotPath, unsigned int kSize, const QString &farmerPrivkeyPath)
{
    QByteArray plotPathBytes = plotPath.toUtf8();
    QByteArray farmerPrivkeyPathBytes = farmerPrivkeyPath.toUtf8();
    
    int result = archivas_farmer_create_plot(
        const_cast<char*>(plotPathBytes.constData()),
        kSize,
        const_cast<char*>(farmerPrivkeyPathBytes.constData())
    );
    
    return result == 0;
}

void ArchivasNodeManager::logCallback(char* level, char* message)
{
    if (s_instance) {
        // Thread-safe signal emission: Go callback might be called from any goroutine
        QString levelStr = QString::fromUtf8(level);
        QString messageStr = QString::fromUtf8(message);
        QMetaObject::invokeMethod(s_instance, "emitNodeLog", Qt::QueuedConnection,
                                  Q_ARG(QString, levelStr),
                                  Q_ARG(QString, messageStr));
    }
}

void ArchivasNodeManager::farmerLogCallback(char* level, char* message)
{
    if (s_instance) {
        // Thread-safe signal emission: Go callback might be called from any goroutine
        QString levelStr = QString::fromUtf8(level);
        QString messageStr = QString::fromUtf8(message);
        QMetaObject::invokeMethod(s_instance, "emitFarmerLog", Qt::QueuedConnection,
                                  Q_ARG(QString, levelStr),
                                  Q_ARG(QString, messageStr));
    }
}

void ArchivasNodeManager::emitNodeLog(const QString &level, const QString &message)
{
    emit nodeLog(level, message);
}

void ArchivasNodeManager::emitFarmerLog(const QString &level, const QString &message)
{
    emit farmerLog(level, message);
}

void ArchivasNodeManager::updateStatus()
{
    emit statusUpdated();
}

