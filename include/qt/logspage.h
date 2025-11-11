#ifndef LOGSPAGE_H
#define LOGSPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include "archivasnodemanager.h"

class LogsPage : public QWidget
{
    Q_OBJECT

public:
    explicit LogsPage(ArchivasNodeManager* nodeManager, QWidget *parent = nullptr);
    ~LogsPage();
    
    // Public methods to add logs (called from MainWindow)
    void addNodeLog(const QString &level, const QString &message);
    void addFarmerLog(const QString &level, const QString &message);

private slots:
    void onNodeLog(const QString &level, const QString &message);
    void onFarmerLog(const QString &level, const QString &message);
    void onClearNodeLogs();
    void onClearFarmerLogs();
    void onSaveNodeLogs();
    void onSaveFarmerLogs();
    void onSearchChanged(const QString& text);

private:
    void setupUi();

    ArchivasNodeManager* m_nodeManager;
    QTabWidget* m_tabWidget;
    QPlainTextEdit* m_nodeLogs;
    QPlainTextEdit* m_farmerLogs;
    QLineEdit* m_searchEdit;
    QCheckBox* m_autoScrollCheck;
    bool m_autoScroll;
};

#endif // LOGSPAGE_H

