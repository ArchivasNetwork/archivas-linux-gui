#ifndef FARMERPAGE_H
#define FARMERPAGE_H

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

class FarmerPage : public QWidget
{
    Q_OBJECT

public:
    explicit FarmerPage(ArchivasNodeManager* nodeManager, ConfigManager* configManager, QWidget *parent = nullptr);
    ~FarmerPage();

private slots:
    void onStartFarmer();
    void onStopFarmer();
    void onRestartFarmer();
    void onCreatePlot();
    void onFarmerStarted();
    void onFarmerStopped();
    void onFarmerLog(const QString &level, const QString &message);
    void updateStatus();

private:
    void setupUi();
    void updateControls();

    ArchivasNodeManager* m_nodeManager;
    ConfigManager* m_configManager;

    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_restartButton;
    QPushButton* m_createPlotButton;
    QLabel* m_statusLabel;
    QLabel* m_plotCountLabel;
    QLineEdit* m_plotsPathEdit;
    QLineEdit* m_farmerPrivkeyPathEdit;
    QPlainTextEdit* m_logTextEdit;
    QTimer* m_statusTimer;
};

#endif // FARMERPAGE_H

