#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class MqttReceiver : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString brokerHost READ brokerHost WRITE setBrokerHost NOTIFY brokerHostChanged)
    Q_PROPERTY(int brokerPort READ brokerPort WRITE setBrokerPort NOTIFY brokerPortChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY autoConnectChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit MqttReceiver(QObject *parent = nullptr);

    bool connected() const { return m_connected; }
    QString brokerHost() const { return m_brokerHost; }
    int brokerPort() const { return m_brokerPort; }
    QString username() const { return m_username; }
    QString password() const { return m_password; }
    bool autoConnect() const { return m_autoConnect; }
    QString statusText() const { return m_statusText; }

    void setBrokerHost(const QString &host);
    void setBrokerPort(int port);
    void setUsername(const QString &username);
    void setPassword(const QString &password);
    void setAutoConnect(bool enabled);

    Q_INVOKABLE void connectBroker();
    Q_INVOKABLE void disconnectBroker();

signals:
    void connectedChanged();
    void brokerHostChanged();
    void brokerPortChanged();
    void usernameChanged();
    void passwordChanged();
    void autoConnectChanged();
    void statusTextChanged();
    void logMessage(const QString &text);
    void deviceLocationUpdated(const QString &deviceId, const QString &deviceName,
                               const QString &room, double distance);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onReadyRead();
    void onPingTimer();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void processBuffer();
    void processPacket(quint8 typeAndFlags, const QByteArray &data);
    void handlePublish(quint8 flags, const QByteArray &data);
    void setStatusText(const QString &text);
    void addLog(const QString &text);

    static QByteArray encodeLength(int len);
    static QByteArray encodeString(const QString &s);
    QByteArray buildConnect() const;
    QByteArray buildSubscribe(const QString &topic);
    static QByteArray buildPing();

    QTcpSocket m_socket;
    QTimer     m_pingTimer;
    QByteArray m_buffer;
    QString    m_brokerHost;
    int        m_brokerPort = 1883;
    QString    m_username;
    QString    m_password;
    bool       m_connected   = false;
    bool       m_autoConnect = false;
    QString    m_statusText  = "未接続";
    quint16    m_packetId    = 1;
};
