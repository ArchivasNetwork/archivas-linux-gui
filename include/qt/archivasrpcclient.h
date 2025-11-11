#ifndef ARCHIVASRPCCLIENT_H
#define ARCHIVASRPCCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QList>
#include <QJsonObject>

struct ChainTip {
    QString height;
    QString hash;
    QString difficulty;
    QString timestamp;
};

struct BlockInfo {
    QString height;
    QString hash;
    QString farmer;
    int txCount;
    QString timestamp;
    QString difficulty;
};

struct TransactionInfo {
    QString hash;
    QString from;
    QString to;
    QString amount;
    QString fee;
    QString height;
    QString timestamp;
};

struct AccountInfo {
    QString address;
    QString balance;
    QString nonce;
};

class ArchivasRpcClient : public QObject
{
    Q_OBJECT

public:
    explicit ArchivasRpcClient(QObject *parent = nullptr);
    ~ArchivasRpcClient();

    void setBaseUrl(const QString& url);
    void setFallbackUrl(const QString& url);
    QString getBaseUrl() const { return m_baseUrl; }

    // Async RPC methods
    void getChainTip();
    void getRecentBlocks(int limit = 20);
    void getRecentTransactions(int limit = 50);
    void getAccount(const QString& address);
    void submitTransaction(const QByteArray& txData);

    bool isConnected() const { return m_connected; }

signals:
    void chainTipUpdated(const ChainTip& tip);
    void blocksUpdated(const QList<BlockInfo>& blocks);
    void transactionsUpdated(const QList<TransactionInfo>& txs);
    void accountUpdated(const AccountInfo& account);
    void connected();
    void disconnected();
    void error(const QString& message);

private slots:
    void onReplyFinished(QNetworkReply* reply);
    void onNetworkError(QNetworkReply::NetworkError error);

private:
    QNetworkAccessManager* m_networkManager;
    QString m_baseUrl;
    QString m_fallbackUrl;
    bool m_connected;
    bool m_usingFallback;

    QNetworkReply* makeGetRequest(const QString& endpoint);
    QNetworkReply* makePostRequest(const QString& endpoint, const QByteArray& data);
    ChainTip parseChainTip(const QJsonObject& json);
    QList<BlockInfo> parseBlocks(const QJsonArray& jsonArray);
    QList<TransactionInfo> parseTransactions(const QJsonArray& jsonArray);
    AccountInfo parseAccount(const QJsonObject& json);
    void checkConnection();
};

#endif // ARCHIVASRPCCLIENT_H

