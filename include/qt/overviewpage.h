#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QTimer>
#include "archivasrpcclient.h"
#include "archivasnodemanager.h"

class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(ArchivasRpcClient* rpcClient, ArchivasNodeManager* nodeManager, QWidget *parent = nullptr);
    ~OverviewPage();

private slots:
    void onChainTipUpdated(const ChainTip& tip);
    void onNodeStatusChanged();
    void onFarmerStatusChanged();
    void onRpcConnected();
    void onRpcDisconnected();
    void updateDisplay();

private:
    void setupUi();
    void updateStatusIndicators();

    ArchivasRpcClient* m_rpcClient;
    ArchivasNodeManager* m_nodeManager;

    // UI Elements
    QLabel* m_chainHeightLabel;
    QLabel* m_chainHashLabel;
    QLabel* m_difficultyLabel;
    QLabel* m_timestampLabel;
    QLabel* m_nodeStatusLabel;
    QLabel* m_farmerStatusLabel;
    QLabel* m_networkLabel;
    QLabel* m_rpcStatusLabel;

    QTimer* m_updateTimer;
    ChainTip m_currentTip;
    bool m_nodeRunning;
    bool m_farmerRunning;
    bool m_rpcConnected;
};

#endif // OVERVIEWPAGE_H

