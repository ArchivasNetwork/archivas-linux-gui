#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTimer>
#include "archivasnodemanager.h"
#include "archivasrpcclient.h"
#include "configmanager.h"
#include "overviewpage.h"
#include "nodepage.h"
#include "farmerpage.h"
#include "blockspage.h"
#include "transactionspage.h"
#include "logspage.h"

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QMenuBar;
class QStatusBar;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onPageChanged(int index);
    void onNodeStatusChanged();
    void onFarmerStatusChanged();
    void onChainTipUpdated(const ChainTip& tip);
    void onRpcConnected();
    void onRpcDisconnected();
    void showSettings();
    void about();

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void setupSidebar();
    void setupPages();
    void updateStatusBar();
    void startPolling();
    QString extractGenesisFile(); // Extract genesis file from Qt resources

    // UI Components
    QWidget* m_centralWidget;
    QSplitter* m_splitter;
    QListWidget* m_sidebar;
    QStackedWidget* m_stackedWidget;

    // Pages
    OverviewPage* m_overviewPage;
    NodePage* m_nodePage;
    FarmerPage* m_farmerPage;
    BlocksPage* m_blocksPage;
    TransactionsPage* m_transactionsPage;
    LogsPage* m_logsPage;

    // Core components
    ArchivasNodeManager* m_nodeManager;
    ArchivasRpcClient* m_rpcClient;
    ConfigManager* m_configManager;

    // Status
    QTimer* m_pollTimer;
    bool m_nodeRunning;
    bool m_farmerRunning;
    bool m_rpcConnected;

    // Menu actions
    QAction* m_settingsAction;
    QAction* m_aboutAction;
    QAction* m_exitAction;
};

#endif // MAINWINDOW_H

