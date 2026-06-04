#include "MqttReceiver.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDateTime>
#include <QUrl>

// --- MQTT 3.1.1 パケット構築 ---

QByteArray MqttReceiver::encodeLength(int len) {
    QByteArray result;
    do {
        quint8 b = len % 128;
        len /= 128;
        if (len > 0) b |= 0x80;
        result.append(static_cast<char>(b));
    } while (len > 0);
    return result;
}

QByteArray MqttReceiver::encodeString(const QString &s) {
    const QByteArray utf8 = s.toUtf8();
    QByteArray result;
    result.append(static_cast<char>(utf8.size() >> 8));
    result.append(static_cast<char>(utf8.size() & 0xFF));
    result.append(utf8);
    return result;
}

QByteArray MqttReceiver::buildConnect() const {
    // Connect Flags: Clean Session(bit1) + Username(bit7) + Password(bit6)
    quint8 flags = 0x02;
    if (!m_username.isEmpty()) flags |= 0x80;
    if (!m_username.isEmpty() && !m_password.isEmpty()) flags |= 0x40;

    QByteArray varHeader;
    varHeader.append(encodeString("MQTT")); // プロトコル名
    varHeader.append(static_cast<char>(0x04)); // レベル 3.1.1
    varHeader.append(static_cast<char>(flags));
    varHeader.append(static_cast<char>(0x00)); // Keepalive MSB
    varHeader.append(static_cast<char>(60));   // Keepalive 60s

    QByteArray payload;
    payload.append(encodeString("ibeacon-scanner-qt"));
    if (!m_username.isEmpty()) {
        payload.append(encodeString(m_username));
        if (!m_password.isEmpty())
            payload.append(encodeString(m_password));
    }

    QByteArray packet;
    packet.append(static_cast<char>(0x10));
    packet.append(encodeLength(varHeader.size() + payload.size()));
    packet.append(varHeader);
    packet.append(payload);
    return packet;
}

QByteArray MqttReceiver::buildSubscribe(const QString &topic) {
    QByteArray body;
    body.append(static_cast<char>(m_packetId >> 8));
    body.append(static_cast<char>(m_packetId & 0xFF));
    body.append(encodeString(topic));
    body.append(static_cast<char>(0x00)); // QoS 0

    QByteArray packet;
    packet.append(static_cast<char>(0x82));
    packet.append(encodeLength(body.size()));
    packet.append(body);
    return packet;
}

QByteArray MqttReceiver::buildPing() {
    return QByteArray("\xC0\x00", 2);
}

// --- MqttReceiver ---

MqttReceiver::MqttReceiver(QObject *parent) : QObject(parent) {
    QSettings s;
    m_brokerHost   = s.value("mqtt/host", "").toString();
    m_brokerPort   = s.value("mqtt/port", 1883).toInt();
    m_username     = s.value("mqtt/username", "").toString();
    m_password     = s.value("mqtt/password", "").toString();
    m_autoConnect  = s.value("mqtt/autoConnect", false).toBool();

    if (m_autoConnect && !m_brokerHost.isEmpty())
        QTimer::singleShot(0, this, &MqttReceiver::connectBroker);

    connect(&m_socket, &QTcpSocket::connected,    this, &MqttReceiver::onSocketConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &MqttReceiver::onSocketDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead,    this, &MqttReceiver::onReadyRead);
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, &MqttReceiver::onSocketError);

    m_pingTimer.setInterval(30000);
    connect(&m_pingTimer, &QTimer::timeout, this, &MqttReceiver::onPingTimer);
}

QString MqttReceiver::brokerUrl() const {
    if (m_brokerHost.isEmpty()) return {};
    return QString("mqtt://%1:%2").arg(m_brokerHost).arg(m_brokerPort);
}

void MqttReceiver::setBrokerUrl(const QString &url) {
    QString normalized = url.trimmed();
    if (normalized.isEmpty()) {
        const bool changed = !m_brokerHost.isEmpty() || m_brokerPort != 1883;
        m_brokerHost.clear();
        m_brokerPort = 1883;
        QSettings s;
        s.setValue("mqtt/url", "");
        s.setValue("mqtt/host", "");
        s.setValue("mqtt/port", m_brokerPort);
        if (changed) {
            emit brokerUrlChanged();
            emit brokerHostChanged();
            emit brokerPortChanged();
        }
        return;
    }

    if (!normalized.contains("://"))
        normalized.prepend("mqtt://");

    const QUrl parsed(normalized);
    QString host = parsed.host();
    int port = parsed.port(1883);

    if (host.isEmpty()) {
        const QString withoutScheme = normalized.mid(normalized.indexOf("://") + 3);
        const QStringList parts = withoutScheme.split(':');
        host = parts.value(0).trimmed();
        if (parts.size() > 1) {
            bool ok = false;
            const int parsedPort = parts.value(1).toInt(&ok);
            if (ok) port = parsedPort;
        }
    }

    port = qBound(1, port, 65535);
    const bool hostChanged = m_brokerHost != host;
    const bool portChanged = m_brokerPort != port;
    if (!hostChanged && !portChanged) return;

    m_brokerHost = host;
    m_brokerPort = port;

    QSettings s;
    s.setValue("mqtt/url", brokerUrl());
    s.setValue("mqtt/host", m_brokerHost);
    s.setValue("mqtt/port", m_brokerPort);

    emit brokerUrlChanged();
    if (hostChanged) emit brokerHostChanged();
    if (portChanged) emit brokerPortChanged();
}
void MqttReceiver::setBrokerHost(const QString &host) {
    if (m_brokerHost == host) return;
    m_brokerHost = host;
    QSettings().setValue("mqtt/host", host);
    emit brokerHostChanged();
}

void MqttReceiver::setBrokerPort(int port) {
    if (m_brokerPort == port) return;
    m_brokerPort = port;
    QSettings().setValue("mqtt/port", port);
    emit brokerPortChanged();
}

void MqttReceiver::setUsername(const QString &username) {
    if (m_username == username) return;
    m_username = username;
    QSettings().setValue("mqtt/username", username);
    emit usernameChanged();
}

void MqttReceiver::setPassword(const QString &password) {
    if (m_password == password) return;
    m_password = password;
    QSettings().setValue("mqtt/password", password);
    emit passwordChanged();
}

void MqttReceiver::setAutoConnect(bool enabled) {
    if (m_autoConnect == enabled) return;
    m_autoConnect = enabled;
    QSettings().setValue("mqtt/autoConnect", enabled);
    emit autoConnectChanged();
}

void MqttReceiver::connectBroker() {
    if (m_brokerHost.isEmpty()) {
        addLog("[MQTT] ⚠ URLが未入力です");
        setStatusText("URLを入力してください");
        return;
    }
    m_buffer.clear();
    addLog(QString("[MQTT] 接続開始 → %1").arg(brokerUrl()));
    if (!m_username.isEmpty())
        addLog(QString("[MQTT] 認証あり (user: %1)").arg(m_username));
    else
        addLog("[MQTT] 認証なし");
    setStatusText("接続中...");
    m_socket.connectToHost(m_brokerHost, static_cast<quint16>(m_brokerPort));
}

void MqttReceiver::disconnectBroker() {
    m_pingTimer.stop();
    addLog("[MQTT] 切断要求");
    m_socket.write(QByteArray("\xE0\x00", 2));
    m_socket.disconnectFromHost();
}

void MqttReceiver::onSocketConnected() {
    addLog("[MQTT] TCP接続確立 → CONNECTパケット送信");
    m_socket.write(buildConnect());
}

void MqttReceiver::onSocketDisconnected() {
    m_pingTimer.stop();
    m_connected = false;
    emit connectedChanged();
    addLog("[MQTT] 切断");
    setStatusText("切断");
}

void MqttReceiver::onSocketError(QAbstractSocket::SocketError) {
    m_connected = false;
    emit connectedChanged();
    const QString msg = m_socket.errorString();
    addLog("[MQTT] ⚠ TCPエラー: " + msg);
    setStatusText("エラー: " + msg);
}

void MqttReceiver::onReadyRead() {
    m_buffer.append(m_socket.readAll());
    processBuffer();
}

void MqttReceiver::onPingTimer() {
    m_socket.write(buildPing());
}

void MqttReceiver::processBuffer() {
    while (m_buffer.size() >= 2) {
        int pos = 1, remaining = 0, multiplier = 1;
        bool lengthComplete = false;

        while (pos < m_buffer.size() && pos <= 4) {
            quint8 b = static_cast<quint8>(m_buffer[pos++]);
            remaining += (b & 0x7F) * multiplier;
            multiplier *= 128;
            if (!(b & 0x80)) { lengthComplete = true; break; }
        }
        if (!lengthComplete) break;

        const int total = pos + remaining;
        if (m_buffer.size() < total) break;

        processPacket(static_cast<quint8>(m_buffer[0]), m_buffer.mid(pos, remaining));
        m_buffer.remove(0, total);
    }
}

void MqttReceiver::processPacket(quint8 typeAndFlags, const QByteArray &data) {
    const quint8 type = typeAndFlags >> 4;
    switch (type) {
    case 2: { // CONNACK
        const quint8 returnCode = data.size() >= 2 ? static_cast<quint8>(data[1]) : 0xFF;
        if (returnCode == 0x00) {
            m_connected = true;
            emit connectedChanged();
            const QString msg = QString("接続済み: %1").arg(brokerUrl());
            addLog("[MQTT] CONNACK: 接続承認");
            addLog("[MQTT] espresense/devices/# をサブスクライブ");
            setStatusText(msg);
            m_socket.write(buildSubscribe("espresense/devices/#"));
            m_packetId++;
            m_pingTimer.start();
        } else {
            static const QMap<quint8, QString> errors = {
                {1, "プロトコルバージョン不正"},
                {2, "クライアントID不正"},
                {3, "ブローカー使用不可"},
                {4, "ユーザー名またはパスワードが間違っています"},
                {5, "接続が許可されていません"},
            };
            const QString reason = errors.value(returnCode,
                                                 QString("不明なエラー(code=%1)").arg(returnCode));
            addLog("[MQTT] ⚠ CONNACK 拒否: " + reason);
            setStatusText("接続拒否: " + reason);
        }
        break;
    }
    case 3: // PUBLISH
        handlePublish(typeAndFlags & 0x0F, data);
        break;
    case 9: // SUBACK
        addLog("[MQTT] SUBACK: サブスクライブ完了");
        break;
    case 13: // PINGRESP
        break;
    default:
        break;
    }
}

void MqttReceiver::handlePublish(quint8 flags, const QByteArray &data) {
    if (data.size() < 2) return;
    const int topicLen = (static_cast<quint8>(data[0]) << 8) | static_cast<quint8>(data[1]);
    if (data.size() < 2 + topicLen) return;
    const QString topic = QString::fromUtf8(data.mid(2, topicLen));

    const quint8 qos = (flags >> 1) & 0x03;
    const int payloadStart = 2 + topicLen + (qos > 0 ? 2 : 0);
    if (data.size() <= payloadStart) return;

    const QJsonObject json = QJsonDocument::fromJson(data.mid(payloadStart)).object();
    if (json.isEmpty()) return;

    const QStringList parts = topic.split('/');
    if (parts.size() < 3) return;

    const QString deviceId   = parts.last();
    const QString deviceName = json.value("name").toString(deviceId);
    const QString room       = json.value("room").toString();
    const double  distance   = json.value("distance").toDouble(-1.0);

    if (room.isEmpty()) return;
    emit deviceLocationUpdated(deviceId, deviceName, room, distance);
}

void MqttReceiver::addLog(const QString &text) {
    emit logMessage(text);
}

void MqttReceiver::setStatusText(const QString &text) {
    m_statusText = text;
    emit statusTextChanged();
}
