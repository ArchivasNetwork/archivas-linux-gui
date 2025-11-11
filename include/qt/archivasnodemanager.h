#ifndef ARCHIVAS_NODE_MANAGER_H
#define ARCHIVAS_NODE_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QString>

extern "C" {
#include "go/bridge/node.h"
#include "go/bridge/farmer.h"
}

class ArchivasNodeManager : public QObject {
    Q_OBJECT

public:
    explicit ArchivasNodeManager(QObject *parent = nullptr);
    ~ArchivasNodeManager();

    // Node methods
    bool startNode(const QString &networkId, const QString &rpcBind, const QString &dataDir, const QString &bootnodes, const QString &genesisPath);
    void stopNode();
    bool isNodeRunning() const;

    // Farmer methods
    bool startFarmer(const QString &nodeUrl, const QString &plotsPath, const QString &farmerPrivkey);
    void stopFarmer();
    bool isFarmerRunning() const;
    bool createPlot(const QString &plotPath, unsigned int kSize, const QString &farmerPrivkeyPath);

    // Status queries
    int getCurrentHeight() const;
    QString getTipHash() const;
    int getPeerCount() const;
    int getPlotCount() const;
    QString getLastProof() const;

signals:
    void nodeStarted();
    void nodeStopped();
    void nodeLog(const QString &level, const QString &message);
    void farmerStarted();
    void farmerStopped();
    void farmerLog(const QString &level, const QString &message);
    void statusUpdated();

private slots:
    void updateStatus();
    void emitNodeLog(const QString &level, const QString &message);
    void emitFarmerLog(const QString &level, const QString &message);

private:
    QTimer *m_statusTimer;
    static void logCallback(char* level, char* message);
    static void farmerLogCallback(char* level, char* message);
    static ArchivasNodeManager* s_instance;
};

#endif // ARCHIVAS_NODE_MANAGER_H
