#ifndef BLOCKSPAGE_H
#define BLOCKSPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include "archivasrpcclient.h"

class BlocksPage : public QWidget
{
    Q_OBJECT

public:
    explicit BlocksPage(ArchivasRpcClient* rpcClient, QWidget *parent = nullptr);
    ~BlocksPage();

private slots:
    void onBlocksUpdated(const QList<BlockInfo>& blocks);
    void onTableDoubleClicked(int row, int column);

private:
    void setupUi();
    void updateTable(const QList<BlockInfo>& blocks);

    ArchivasRpcClient* m_rpcClient;
    QTableWidget* m_tableWidget;
    QTimer* m_refreshTimer;
};

#endif // BLOCKSPAGE_H

