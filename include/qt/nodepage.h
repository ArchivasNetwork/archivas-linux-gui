#ifndef NODEPAGE_H
#define NODEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QGroupBox>
#include "archivasnodemanager.h"
#include "configmanager.h"

class NodePage : public QWidget
{
    Q_OBJECT

public:
    explicit NodePage(ArchivasNodeManager* nodeManager, ConfigManager* configManager, QWidget *parent = nullptr);
    ~NodePage();

private slots:
    void onStartNode();
    void onStopNode();
    void onRestartNode();
    void onNodeStarted();
    void onNodeStopped();
    void onNodeLog(const QString &level, const QString &message);
    void updateStatus();

private:
    void setupUi();
    void updateControls();
    QString extractGenesisFile(); // Extract genesis file from Qt resources

    ArchivasNodeManager* m_nodeManager;
    ConfigManager* m_configManager;

    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_restartButton;
    QLabel* m_statusLabel;
    QLabel* m_peerCountLabel;
    QPlainTextEdit* m_logTextEdit;
    QTimer* m_statusTimer;
};

#endif // NODEPAGE_H

