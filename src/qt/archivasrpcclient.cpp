#include "archivasrpcclient.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

ArchivasRpcClient::ArchivasRpcClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(nullptr)
    , m_baseUrl("http://127.0.0.1:8080")
    , m_fallbackUrl("https://seed.archivas.ai")
    , m_connected(false)
    , m_usingFallback(false)
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &ArchivasRpcClient::onReplyFinished);
}

ArchivasRpcClient::~ArchivasRpcClient()
{
}

void ArchivasRpcClient::setBaseUrl(const QString& url)
{
    m_baseUrl = url.trimmed();
    // Remove trailing slash
    while (m_baseUrl.endsWith('/')) {
        m_baseUrl.chop(1);
    }
    m_usingFallback = false;
    // Don't check connection immediately - let the caller do it when ready
}

void ArchivasRpcClient::setFallbackUrl(const QString& url)
{
    m_fallbackUrl = url.trimmed();
    // Remove trailing slash
    while (m_fallbackUrl.endsWith('/')) {
        m_fallbackUrl.chop(1);
    }
}

QNetworkReply* ArchivasRpcClient::makeGetRequest(const QString& endpoint)
{
    QString endpointClean = endpoint.trimmed();
    // Remove leading slash if present
    if (endpointClean.startsWith('/')) {
        endpointClean = endpointClean.mid(1);
    }
    QString url = m_baseUrl + "/" + endpointClean;
    QUrl requestUrl(url);
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "Archivas-Core-GUI/1.0");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &ArchivasRpcClient::onNetworkError);

    return reply;
}

QNetworkReply* ArchivasRpcClient::makePostRequest(const QString& endpoint, const QByteArray& data)
{
    QString endpointClean = endpoint.trimmed();
    // Remove leading slash if present
    if (endpointClean.startsWith('/')) {
        endpointClean = endpointClean.mid(1);
    }
    QString url = m_baseUrl + "/" + endpointClean;
    QUrl requestUrl(url);
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "Archivas-Core-GUI/1.0");

    QNetworkReply* reply = m_networkManager->post(request, data);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &ArchivasRpcClient::onNetworkError);

    return reply;
}

void ArchivasRpcClient::getChainTip()
{
    makeGetRequest("chainTip");
}

void ArchivasRpcClient::getRecentBlocks(int limit)
{
    makeGetRequest(QString("blocks/recent?limit=%1").arg(limit));
}

void ArchivasRpcClient::getRecentTransactions(int limit)
{
    makeGetRequest(QString("tx/recent?limit=%1").arg(limit));
}

void ArchivasRpcClient::getAccount(const QString& address)
{
    makeGetRequest(QString("account/%1").arg(address));
}

void ArchivasRpcClient::submitTransaction(const QByteArray& txData)
{
    makePostRequest("submit", txData);
}

void ArchivasRpcClient::checkConnection()
{
    // Try to ping the base URL
    getChainTip();
}

void ArchivasRpcClient::onReplyFinished(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        // Try fallback if not already using it
        if (!m_usingFallback && !m_fallbackUrl.isEmpty()) {
            QString originalUrl = m_baseUrl;
            m_baseUrl = m_fallbackUrl;
            m_usingFallback = true;
            // Retry the request - extract endpoint from the original URL
            QUrl originalRequestUrl = reply->request().url();
            QString endpoint = originalRequestUrl.path();
            if (endpoint.startsWith('/')) {
                endpoint = endpoint.mid(1);
            }
            if (originalRequestUrl.hasQuery()) {
                endpoint += "?" + originalRequestUrl.query();
            }
            makeGetRequest(endpoint);
            reply->deleteLater();
            return;
        }

        emit error(reply->errorString());
        emit disconnected();
        m_connected = false;
        reply->deleteLater();
        return;
    }

    if (!m_connected) {
        m_connected = true;
        emit connected();
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        emit error("Failed to parse JSON response");
        reply->deleteLater();
        return;
    }

    QString path = reply->url().path();

    if (path.contains("chainTip")) {
        if (doc.isObject()) {
            ChainTip tip = parseChainTip(doc.object());
            emit chainTipUpdated(tip);
        }
    } else if (path.contains("blocks/recent")) {
        if (doc.isArray()) {
            QList<BlockInfo> blocks = parseBlocks(doc.array());
            emit blocksUpdated(blocks);
        } else if (doc.isObject() && doc.object().contains("blocks")) {
            QList<BlockInfo> blocks = parseBlocks(doc.object()["blocks"].toArray());
            emit blocksUpdated(blocks);
        }
    } else if (path.contains("tx/recent")) {
        if (doc.isArray()) {
            QList<TransactionInfo> txs = parseTransactions(doc.array());
            emit transactionsUpdated(txs);
        } else if (doc.isObject() && doc.object().contains("transactions")) {
            QList<TransactionInfo> txs = parseTransactions(doc.object()["transactions"].toArray());
            emit transactionsUpdated(txs);
        }
    } else if (path.contains("account/")) {
        if (doc.isObject()) {
            AccountInfo account = parseAccount(doc.object());
            emit accountUpdated(account);
        }
    }

    reply->deleteLater();
}

void ArchivasRpcClient::onNetworkError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);
    // Error handling is done in onReplyFinished
}

ChainTip ArchivasRpcClient::parseChainTip(const QJsonObject& json)
{
    ChainTip tip;
    tip.height = json.value("height").toString();
    tip.hash = json.value("hash").toString();
    tip.difficulty = json.value("difficulty").toString();
    tip.timestamp = json.value("timestamp").toString();
    return tip;
}

QList<BlockInfo> ArchivasRpcClient::parseBlocks(const QJsonArray& jsonArray)
{
    QList<BlockInfo> blocks;
    for (const QJsonValue& value : jsonArray) {
        if (value.isObject()) {
            QJsonObject obj = value.toObject();
            BlockInfo block;
            block.height = obj.value("height").toString();
            block.hash = obj.value("hash").toString();
            block.farmer = obj.value("farmer").toString();
            block.txCount = obj.value("txCount").toInt();
            block.timestamp = obj.value("timestamp").toString();
            block.difficulty = obj.value("difficulty").toString();
            blocks.append(block);
        }
    }
    return blocks;
}

QList<TransactionInfo> ArchivasRpcClient::parseTransactions(const QJsonArray& jsonArray)
{
    QList<TransactionInfo> txs;
    for (const QJsonValue& value : jsonArray) {
        if (value.isObject()) {
            QJsonObject obj = value.toObject();
            TransactionInfo tx;
            tx.hash = obj.value("hash").toString();
            tx.from = obj.value("from").toString();
            tx.to = obj.value("to").toString();
            tx.amount = obj.value("amount").toString();
            tx.fee = obj.value("fee").toString();
            tx.height = obj.value("height").toString();
            tx.timestamp = obj.value("timestamp").toString();
            txs.append(tx);
        }
    }
    return txs;
}

AccountInfo ArchivasRpcClient::parseAccount(const QJsonObject& json)
{
    AccountInfo account;
    account.address = json.value("address").toString();
    account.balance = json.value("balance").toString();
    account.nonce = json.value("nonce").toString();
    return account;
}

