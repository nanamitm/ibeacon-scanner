#pragma once

#include <QObject>
#include <QHash>
#include <QNetworkAccessManager>
#include <QTimer>

class QNetworkReply;

class HaReceiver : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString accessToken READ accessToken WRITE setAccessToken NOTIFY accessTokenChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY autoConnectChanged)
    Q_PROPERTY(int pollInterval READ pollInterval WRITE setPollInterval NOTIFY pollIntervalChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit HaReceiver(QObject *parent = nullptr);

    bool connected() const { return m_connected; }
    QString serverUrl() const { return m_serverUrl; }
    QString accessToken() const { return m_accessToken; }
    bool autoConnect() const { return m_autoConnect; }
    int pollInterval() const { return m_pollTimer.interval(); }
    QString statusText() const { return m_statusText; }

    void setServerUrl(const QString &url);
    void setAccessToken(const QString &token);
    void setAutoConnect(bool enabled);
    void setPollInterval(int ms);

    Q_INVOKABLE void connectServer();
    Q_INVOKABLE void disconnectServer();
    Q_INVOKABLE void pollNow();

signals:
    void connectedChanged();
    void serverUrlChanged();
    void accessTokenChanged();
    void autoConnectChanged();
    void pollIntervalChanged();
    void statusTextChanged();
    void logMessage(const QString &text);
    void deviceLocationUpdated(const QString &deviceId, const QString &deviceName,
                               const QString &room, double distance);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QUrl statesUrl() const;
    void fetchStates();
    void processStates(const QByteArray &payload);
    void setConnected(bool connected);
    void setStatusText(const QString &text);
    void addLog(const QString &text);

    QNetworkAccessManager m_network;
    QTimer m_pollTimer;
    QHash<QString, QString> m_lastRoomByDevice;
    QString m_serverUrl;
    QString m_accessToken;
    bool m_connected = false;
    bool m_autoConnect = false;
    QString m_statusText = "未接続";
};

