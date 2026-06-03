#include "HaReceiver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

namespace {
constexpr int MinPollIntervalMs = 5000;
constexpr int MaxPollIntervalMs = 60000;

QString trimmedUrl(QString url) {
    url = url.trimmed();
    while (url.endsWith('/'))
        url.chop(1);
    return url;
}

QString friendlyDeviceName(QString friendlyName, const QString &fallback) {
    friendlyName = friendlyName.trimmed();
    if (friendlyName.endsWith(" Area", Qt::CaseInsensitive))
        friendlyName.chop(5);
    return friendlyName.isEmpty() ? fallback : friendlyName;
}
}

HaReceiver::HaReceiver(QObject *parent) : QObject(parent) {
    QSettings s;
    m_serverUrl = s.value("homeAssistant/url", "").toString();
    m_accessToken = s.value("homeAssistant/token", "").toString();
    m_autoConnect = s.value("homeAssistant/autoConnect", false).toBool();
    m_pollTimer.setInterval(s.value("homeAssistant/pollInterval", 10000).toInt());
    setPollInterval(m_pollTimer.interval());

    connect(&m_pollTimer, &QTimer::timeout, this, &HaReceiver::fetchStates);
    connect(&m_network, &QNetworkAccessManager::finished, this, &HaReceiver::onReplyFinished);

    if (m_autoConnect && !m_serverUrl.isEmpty() && !m_accessToken.isEmpty())
        QTimer::singleShot(0, this, &HaReceiver::connectServer);
}

void HaReceiver::setServerUrl(const QString &url) {
    const QString normalized = trimmedUrl(url);
    if (m_serverUrl == normalized) return;
    m_serverUrl = normalized;
    QSettings().setValue("homeAssistant/url", m_serverUrl);
    emit serverUrlChanged();
}

void HaReceiver::setAccessToken(const QString &token) {
    if (m_accessToken == token) return;
    m_accessToken = token;
    QSettings().setValue("homeAssistant/token", token);
    emit accessTokenChanged();
}

void HaReceiver::setAutoConnect(bool enabled) {
    if (m_autoConnect == enabled) return;
    m_autoConnect = enabled;
    QSettings().setValue("homeAssistant/autoConnect", enabled);
    emit autoConnectChanged();
}

void HaReceiver::setPollInterval(int ms) {
    const int bounded = qBound(MinPollIntervalMs, ms, MaxPollIntervalMs);
    if (m_pollTimer.interval() == bounded) return;
    m_pollTimer.setInterval(bounded);
    QSettings().setValue("homeAssistant/pollInterval", bounded);
    emit pollIntervalChanged();
}

void HaReceiver::connectServer() {
    if (m_serverUrl.isEmpty()) {
        addLog("[HA] ホストが未入力です");
        setStatusText("URLを入力してください");
        return;
    }
    if (m_accessToken.isEmpty()) {
        addLog("[HA] Long-Lived Access Token が未入力です");
        setStatusText("トークンを入力してください");
        return;
    }
    if (!statesUrl().isValid()) {
        addLog("[HA] URLが不正です: " + m_serverUrl);
        setStatusText("URLが不正です");
        return;
    }

    addLog("[HA] Bermuda連携を開始: " + m_serverUrl);
    setStatusText("接続中...");
    m_pollTimer.start();
    fetchStates();
}

void HaReceiver::disconnectServer() {
    m_pollTimer.stop();
    setConnected(false);
    setStatusText("切断");
    addLog("[HA] Bermuda連携を停止");
}

void HaReceiver::pollNow() {
    fetchStates();
}

QUrl HaReceiver::statesUrl() const {
    return QUrl(trimmedUrl(m_serverUrl) + "/api/states");
}

void HaReceiver::fetchStates() {
    if (m_serverUrl.isEmpty() || m_accessToken.isEmpty()) return;

    QNetworkRequest request(statesUrl());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    m_network.get(request);
}

void HaReceiver::onReplyFinished(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setConnected(false);
        const QString message = reply->errorString();
        addLog("[HA] エラー: " + message);
        setStatusText("エラー: " + message);
        return;
    }

    const QByteArray payload = reply->readAll();
    processStates(payload);
    if (!m_connected) {
        addLog("[HA] Home Assistant 接続確認");
        setConnected(true);
    }
    setStatusText(QString("接続済み: %1").arg(m_serverUrl));
}

void HaReceiver::processStates(const QByteArray &payload) {
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isArray()) {
        addLog("[HA] /api/states の応答をJSON配列として読めませんでした");
        return;
    }

    int matched = 0;
    for (const QJsonValue &value : doc.array()) {
        const QJsonObject stateObject = value.toObject();
        const QString entityId = stateObject.value("entity_id").toString();
        if (!entityId.startsWith("sensor.")) continue;

        const QString state = stateObject.value("state").toString().trimmed();
        if (state.isEmpty() || state == "unknown" || state == "unavailable") continue;

        const QJsonObject attrs = stateObject.value("attributes").toObject();
        const QString currentMac = attrs.value("current_mac").toString().trimmed();
        const QString areaName = attrs.value("area_name").toString().trimmed();
        const QString friendlyName = attrs.value("friendly_name").toString().trimmed();
        const QString deviceClass = attrs.value("device_class").toString().trimmed();

        const bool looksLikeBermudaArea =
            !currentMac.isEmpty()
            && !areaName.isEmpty()
            && friendlyName.endsWith(" Area", Qt::CaseInsensitive)
            && (deviceClass.isEmpty() || deviceClass == "bermuda__custom_device_class");
        if (!looksLikeBermudaArea) continue;

        const QString deviceId = currentMac;
        const QString deviceName = friendlyDeviceName(friendlyName, entityId);
        const QString room = state;
        if (room.isEmpty()) continue;

        matched++;
        if (m_lastRoomByDevice.value(deviceId) == room) continue;
        m_lastRoomByDevice[deviceId] = room;
        addLog(QString("[HA] Bermuda: %1 → %2").arg(deviceName, room));
        emit deviceLocationUpdated(deviceId, deviceName, room, -1.0);
    }

    if (matched == 0)
        addLog("[HA] BermudaのAreaセンサーは見つかりませんでした");
}

void HaReceiver::setConnected(bool connected) {
    if (m_connected == connected) return;
    m_connected = connected;
    emit connectedChanged();
}

void HaReceiver::setStatusText(const QString &text) {
    if (m_statusText == text) return;
    m_statusText = text;
    emit statusTextChanged();
}

void HaReceiver::addLog(const QString &text) {
    emit logMessage(text);
}

