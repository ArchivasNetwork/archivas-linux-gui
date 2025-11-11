#ifndef TRANSACTIONSPAGE_H
#define TRANSACTIONSPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include "archivasrpcclient.h"

class TransactionsPage : public QWidget
{
    Q_OBJECT

public:
    explicit TransactionsPage(ArchivasRpcClient* rpcClient, QWidget *parent = nullptr);
    ~TransactionsPage();

private slots:
    void onTransactionsUpdated(const QList<TransactionInfo>& txs);
    void onTableDoubleClicked(int row, int column);
    void onFilterClicked();

private:
    void setupUi();
    void updateTable(const QList<TransactionInfo>& txs);

    ArchivasRpcClient* m_rpcClient;
    QTableWidget* m_tableWidget;
    QLineEdit* m_filterEdit;
    QTimer* m_refreshTimer;
    QList<TransactionInfo> m_allTransactions;
};

#endif // TRANSACTIONSPAGE_H

