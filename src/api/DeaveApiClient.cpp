#include "DeaveApiClient.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

DeaveApiClient::DeaveApiClient(const QString& baseUrl, QObject* parent)
    : QObject(parent)
    , m_baseUrl(baseUrl)
    , m_timeoutMs(700)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

DeaveApiClient::~DeaveApiClient()
{
}

QString DeaveApiClient::baseUrl() const
{
    return m_baseUrl;
}

void DeaveApiClient::setBaseUrl(const QString& baseUrl)
{
    m_baseUrl = baseUrl;
}

int DeaveApiClient::timeoutMs() const
{
    return m_timeoutMs;
}

void DeaveApiClient::setTimeoutMs(int timeoutMs)
{
    m_timeoutMs = timeoutMs;
}

QUrl DeaveApiClient::buildUrl(const QString& endpointPath) const
{
    QString base = m_baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/')))
    {
        base.chop(1);
    }

    QString rel = endpointPath.trimmed();
    if (!rel.startsWith(QLatin1Char('/')))
    {
        rel.prepend(QLatin1Char('/'));
    }

    return QUrl(base + rel);
}

void DeaveApiClient::attachTimeout(QNetworkReply* reply, int timeoutMs)
{
    if (!reply || timeoutMs <= 0)
    {
        return;
    }

    QTimer* timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply]()
    {
        if (reply->isRunning())
        {
            reply->abort();
        }
    });
    timer->start(timeoutMs);
}

void DeaveApiClient::checkHealth(std::function<void(const DeaveHealthInfo& info)> callback)
{
    const QUrl url = buildUrl(QStringLiteral("/health"));
    QNetworkRequest request(url);
    request.setTransferTimeout(m_timeoutMs);
    request.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = m_networkManager->get(request);
    attachTimeout(reply, m_timeoutMs);

    connect(reply, &QNetworkReply::finished, this, [reply, callback]()
    {
        DeaveHealthInfo info;
        info.available = false;

        if (reply->error() == QNetworkReply::NoError)
        {
            const QByteArray data = reply->readAll();
            const QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject())
            {
                const QJsonObject obj = doc.object();
                info.status = obj.value(QStringLiteral("status")).toString();
                info.apiVersion = obj.value(QStringLiteral("api_version")).toString();
                info.env = obj.value(QStringLiteral("env")).toString();
                info.available = (info.status == QStringLiteral("ok"));
            }
        }

        reply->deleteLater();

        if (callback)
        {
            callback(info);
        }
    });
}

void DeaveApiClient::searchMemory(
    const QString& query,
    int limit,
    std::function<void(bool success, const QList<DeaveSearchHit>& hits, const QString& errorMessage)> callback)
{
    QUrl url = buildUrl(QStringLiteral("/api/v1/memory/search"));
    QUrlQuery queryParams;
    queryParams.addQueryItem(QStringLiteral("q"), query);
    queryParams.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    url.setQuery(queryParams);

    QNetworkRequest request(url);
    request.setTransferTimeout(m_timeoutMs);
    request.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = m_networkManager->get(request);
    attachTimeout(reply, m_timeoutMs);

    connect(reply, &QNetworkReply::finished, this, [reply, callback]()
    {
        if (reply->error() == QNetworkReply::NoError)
        {
            const QByteArray data = reply->readAll();
            const QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isArray())
            {
                QList<DeaveSearchHit> hits;
                const QJsonArray arr = doc.array();
                hits.reserve(arr.size());
                for (const QJsonValue& val : arr)
                {
                    if (val.isObject())
                    {
                        const QJsonObject obj = val.toObject();
                        DeaveSearchHit hit;
                        hit.nodeId = obj.value(QStringLiteral("node_id")).toString();
                        hit.title = obj.value(QStringLiteral("title")).toString();
                        hit.objectType = obj.value(QStringLiteral("object_type")).toString();
                        hit.sensitivity = obj.value(QStringLiteral("sensitivity")).toString();
                        hit.source = obj.value(QStringLiteral("source")).toString();
                        hit.sourcePath = obj.value(QStringLiteral("source_path")).toString();
                        hit.snippet = obj.value(QStringLiteral("snippet")).toString();
                        hit.score = obj.value(QStringLiteral("score")).toDouble();
                        hit.contentSha256 = obj.value(QStringLiteral("content_sha256")).toString();
                        hits.append(hit);
                    }
                }

                reply->deleteLater();
                if (callback)
                {
                    callback(true, hits, QString());
                }
                return;
            }
        }

        const QString errorString = reply->errorString();
        reply->deleteLater();
        if (callback)
        {
            callback(false, {}, errorString);
        }
    });
}

void DeaveApiClient::readMemoryNote(
    const QString& source,
    const QString& path,
    std::function<void(bool success, const DeaveNoteReadResult& note, const QString& errorMessage)> callback)
{
    QUrl url = buildUrl(QStringLiteral("/api/v1/memory/read"));
    QUrlQuery queryParams;
    queryParams.addQueryItem(QStringLiteral("source"), source);
    queryParams.addQueryItem(QStringLiteral("path"), path);
    url.setQuery(queryParams);

    QNetworkRequest request(url);
    request.setTransferTimeout(m_timeoutMs);
    request.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = m_networkManager->get(request);
    attachTimeout(reply, m_timeoutMs);

    connect(reply, &QNetworkReply::finished, this, [reply, callback]()
    {
        if (reply->error() == QNetworkReply::NoError)
        {
            const QByteArray data = reply->readAll();
            const QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject())
            {
                const QJsonObject obj = doc.object();
                DeaveNoteReadResult result;
                result.source = obj.value(QStringLiteral("source")).toString();
                result.path = obj.value(QStringLiteral("path")).toString();
                result.title = obj.value(QStringLiteral("title")).toString();
                result.content = obj.value(QStringLiteral("content")).toString();
                result.contentSha256 = obj.value(QStringLiteral("content_sha256")).toString();
                result.size = obj.value(QStringLiteral("size")).toVariant().toLongLong();
                result.mtime = obj.value(QStringLiteral("mtime")).toDouble();
                result.nodeId = obj.value(QStringLiteral("node_id")).toString();
                result.sensitivity = obj.value(QStringLiteral("sensitivity")).toString();

                reply->deleteLater();
                if (callback)
                {
                    callback(true, result, QString());
                }
                return;
            }
        }

        const QString errorString = reply->errorString();
        reply->deleteLater();
        if (callback)
        {
            callback(false, DeaveNoteReadResult(), errorString);
        }
    });
}

void DeaveApiClient::captureNote(
    const QString& target,
    const QString& text,
    const QString& clientId,
    const QString& token,
    std::function<void(const DeaveCaptureResult& result)> callback)
{
    const QUrl url = buildUrl(QStringLiteral("/api/v1/capture"));
    QNetworkRequest request(url);
    request.setTransferTimeout(m_timeoutMs);
    request.setRawHeader("Accept", "application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!token.trimmed().isEmpty())
    {
        request.setRawHeader("X-Deave-Token", token.trimmed().toLatin1());
    }

    QJsonObject rootObj;
    rootObj.insert(QStringLiteral("target"), target.trimmed());
    rootObj.insert(QStringLiteral("client_id"), clientId.trimmed());
    rootObj.insert(QStringLiteral("text"), text);
    rootObj.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    QJsonObject metaObj;
    metaObj.insert(QStringLiteral("source"), QStringLiteral("study-widgets"));
    metaObj.insert(QStringLiteral("widget"), QStringLiteral("NotesWidget"));
    rootObj.insert(QStringLiteral("metadata"), metaObj);

    const QByteArray postData = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_networkManager->post(request, postData);
    attachTimeout(reply, m_timeoutMs);

    connect(reply, &QNetworkReply::finished, this, [reply, callback]()
    {
        DeaveCaptureResult result;
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.httpStatusCode = statusCode;

        const QByteArray responseData = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(responseData);
        const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject();

        if (statusCode == 200 || statusCode == 201)
        {
            result.success = true;
            result.clientId = obj.value(QStringLiteral("client_id")).toString();
            result.target = obj.value(QStringLiteral("target")).toString();
            result.seq = obj.value(QStringLiteral("seq")).toVariant().toLongLong();
            result.duplicated = obj.value(QStringLiteral("duplicated")).toBool(statusCode == 200);
            result.message = obj.value(QStringLiteral("message")).toString();
        }
        else
        {
            result.success = false;
            if (obj.contains(QStringLiteral("detail")))
            {
                result.errorMessage = obj.value(QStringLiteral("detail")).toString();
            }
            else
            {
                result.errorMessage = reply->errorString();
            }
        }

        reply->deleteLater();
        if (callback)
        {
            callback(result);
        }
    });
}
