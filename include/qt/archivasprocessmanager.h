#ifndef ARCHIVASPROCESSMANAGER_H
#define ARCHIVASPROCESSMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class ArchivasProcessManager : public QObject
{
    Q_OBJECT

public:
    explicit ArchivasProcessManager(QObject *parent = nullptr);
    ~ArchivasProcessManager();

    // Node methods
    bool startNode(const QString& executablePath, const QString& network,
                   const QString& rpcBind, const QString& dataDir,
                   const QString& bootnodes);
    void stopNode();
    bool isNodeRunning() const;
    QString getNodeExecutablePath() const { return m_nodeExecutablePath; }

    // Farmer methods
    bool startFarmer(const QString& executablePath, const QString& plotsPath,
                     const QString& farmerPrivkeyPath, const QString& nodeUrl);
    void stopFarmer();
    bool isFarmerRunning() const;
    QString getFarmerExecutablePath() const { return m_farmerExecutablePath; }

    // Process output
    QString getNodeOutput() const { return m_nodeOutput; }
    QString getFarmerOutput() const { return m_farmerOutput; }

signals:
    void nodeStarted();
    void nodeStopped();
    void nodeError(const QString& error);
    void nodeOutput(const QString& line);
    void farmerStarted();
    void farmerStopped();
    void farmerError(const QString& error);
    void farmerOutput(const QString& line);

private slots:
    void onNodeReadyRead();
    void onNodeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onNodeError(QProcess::ProcessError error);
    void onFarmerReadyRead();
    void onFarmerFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onFarmerError(QProcess::ProcessError error);

private:
    QProcess* m_nodeProcess;
    QProcess* m_farmerProcess;
    QString m_nodeExecutablePath;
    QString m_farmerExecutablePath;
    QString m_nodeOutput;
    QString m_farmerOutput;

    void setupNodeProcess();
    void setupFarmerProcess();
    QStringList buildNodeArgs(const QString& network, const QString& rpcBind,
                             const QString& dataDir, const QString& bootnodes);
    QStringList buildFarmerArgs(const QString& plotsPath,
                               const QString& farmerPrivkeyPath,
                               const QString& nodeUrl);
};

#endif // ARCHIVASPROCESSMANAGER_H

