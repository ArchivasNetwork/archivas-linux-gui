#include "transactionspage.h"
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>

TransactionsPage::TransactionsPage(ArchivasRpcClient* rpcClient, QWidget *parent)
    : QWidget(parent)
    , m_rpcClient(rpcClient)
    , m_tableWidget(nullptr)
    , m_filterEdit(nullptr)
    , m_refreshTimer(nullptr)
{
    setupUi();

    connect(m_rpcClient, &ArchivasRpcClient::transactionsUpdated, this, &TransactionsPage::onTransactionsUpdated);

    // Auto-refresh every 10 seconds
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, [this]() {
        m_rpcClient->getRecentTransactions(50);
    });
    m_refreshTimer->start(10000);

    // Initial load
    m_rpcClient->getRecentTransactions(50);
}

TransactionsPage::~TransactionsPage()
{
}

void TransactionsPage::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Filter bar
    QHBoxLayout* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel("Filter by Address:", this));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Enter address to filter...");
    filterLayout->addWidget(m_filterEdit);
    QPushButton* filterButton = new QPushButton("Filter", this);
    connect(filterButton, &QPushButton::clicked, this, &TransactionsPage::onFilterClicked);
    filterLayout->addWidget(filterButton);
    filterLayout->addStretch();
    mainLayout->addLayout(filterLayout);

    // Table
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(7);
    m_tableWidget->setHorizontalHeaderLabels({"Hash", "From", "To", "Amount (RCHV)", "Fee (RCHV)", "Height", "Timestamp"});
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->setAlternatingRowColors(true);

    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &TransactionsPage::onTableDoubleClicked);

    mainLayout->addWidget(m_tableWidget);
}

void TransactionsPage::onTransactionsUpdated(const QList<TransactionInfo>& txs)
{
    m_allTransactions = txs;
    updateTable(txs);
}

void TransactionsPage::updateTable(const QList<TransactionInfo>& txs)
{
    m_tableWidget->setRowCount(txs.size());

    for (int i = 0; i < txs.size(); ++i) {
        const TransactionInfo& tx = txs[i];

        // Hash (truncated)
        QString shortHash = tx.hash;
        if (shortHash.length() > 16) {
            shortHash = shortHash.left(8) + "..." + shortHash.right(8);
        }
        QTableWidgetItem* hashItem = new QTableWidgetItem(shortHash);
        hashItem->setData(Qt::UserRole, tx.hash);
        hashItem->setToolTip(tx.hash);
        m_tableWidget->setItem(i, 0, hashItem);

        // From (truncated)
        QString shortFrom = tx.from;
        if (shortFrom.length() > 16) {
            shortFrom = shortFrom.left(8) + "..." + shortFrom.right(8);
        }
        QTableWidgetItem* fromItem = new QTableWidgetItem(shortFrom);
        fromItem->setToolTip(tx.from);
        m_tableWidget->setItem(i, 1, fromItem);

        // To (truncated)
        QString shortTo = tx.to;
        if (shortTo.length() > 16) {
            shortTo = shortTo.left(8) + "..." + shortTo.right(8);
        }
        QTableWidgetItem* toItem = new QTableWidgetItem(shortTo);
        toItem->setToolTip(tx.to);
        m_tableWidget->setItem(i, 2, toItem);

        // Amount
        QTableWidgetItem* amountItem = new QTableWidgetItem(tx.amount);
        m_tableWidget->setItem(i, 3, amountItem);

        // Fee
        QTableWidgetItem* feeItem = new QTableWidgetItem(tx.fee);
        m_tableWidget->setItem(i, 4, feeItem);

        // Height
        QTableWidgetItem* heightItem = new QTableWidgetItem(tx.height);
        m_tableWidget->setItem(i, 5, heightItem);

        // Timestamp
        QTableWidgetItem* timestampItem = new QTableWidgetItem(tx.timestamp);
        m_tableWidget->setItem(i, 6, timestampItem);
    }

    m_tableWidget->resizeColumnsToContents();
}

void TransactionsPage::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= m_tableWidget->rowCount()) {
        return;
    }

    // Copy hash to clipboard
    QTableWidgetItem* hashItem = m_tableWidget->item(row, 0);
    if (hashItem) {
        QString fullHash = hashItem->data(Qt::UserRole).toString();
        if (fullHash.isEmpty()) {
            fullHash = hashItem->text();
        }
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText(fullHash);
    }
}

void TransactionsPage::onFilterClicked()
{
    QString filter = m_filterEdit->text().trimmed();
    if (filter.isEmpty()) {
        updateTable(m_allTransactions);
        return;
    }

    QList<TransactionInfo> filtered;
    for (const TransactionInfo& tx : m_allTransactions) {
        if (tx.from.contains(filter, Qt::CaseInsensitive) ||
            tx.to.contains(filter, Qt::CaseInsensitive) ||
            tx.hash.contains(filter, Qt::CaseInsensitive)) {
            filtered.append(tx);
        }
    }

    updateTable(filtered);
}

