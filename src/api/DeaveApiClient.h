#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

struct DeaveHealthInfo
{
    bool available = false;
    QString apiVersion;
    QString env;
    QString status;
};

struct DeaveSearchHit
{
    QString nodeId;
    QString title;
    QString objectType;
    QString sensitivity;
    QString source;
    QString sourcePath;
    QString snippet;
    double score = 0.0;
    QString contentSha256;
};

struct DeaveNoteReadResult
{
    QString source;
    QString path;
    QString title;
    QString content;
    QString contentSha256;
    qint64 size = 0;
    double mtime = 0.0;
    QString nodeId;
    QString sensitivity;
};

class DeaveApiClient : public QObject
{
    Q_OBJECT

public:
    explicit DeaveApiClient(
        const QString& baseUrl = QStringLiteral("http://127.0.0.1:8000"),
        QObject* parent = nullptr
    );
    ~DeaveApiClient() override;

    QString baseUrl() const;
    void setBaseUrl(const QString& baseUrl);

    int timeoutMs() const;
    void setTimeoutMs(int timeoutMs);

    void checkHealth(std::function<void(const DeaveHealthInfo& info)> callback);

    void searchMemory(
        const QString& query,
        int limit,
        std::function<void(bool success, const QList<DeaveSearchHit>& hits, const QString& errorMessage)> callback
    );

    void readMemoryNote(
        const QString& source,
        const QString& path,
        std::function<void(bool success, const DeaveNoteReadResult& note, const QString& errorMessage)> callback
    );

private:
    QUrl buildUrl(const QString& endpointPath) const;
    void attachTimeout(QNetworkReply* reply, int timeoutMs);

    QString m_baseUrl;
    int m_timeoutMs = 700;
    QNetworkAccessManager* m_networkManager = nullptr;
};
