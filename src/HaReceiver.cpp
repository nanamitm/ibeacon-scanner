#include "HaReceiver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonParseError>
#include <QtConcurrent>


namespace {
constexpr int MinPollIntervalMs = 5000;
constexpr int MaxPollIntervalMs = 60000;

QString trimmedUrl(QString url) {
    url = url.trimmed();
    if (!url.isEmpty() && !url.contains("://"))
        url.prepend("http://");
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
    m_serverUrl   = s.value("homeAssistant/url", "").toString();
    m_accessToken = s.value("homeAssistant/token", "").toString();
    m_autoConnect = s.value("homeAssistant/autoConnect", false).toBool();
    m_pollTimer.setInterval(s.value("homeAssistant/pollInterval", 10000).toInt());
    setPollInterval(m_pollTimer.interval());

    connect(&m_pollTimer, &QTimer::timeout, this, &HaReceiver::fetchStates);
    connect(&m_network, &QNetworkAccessManager::finished, this, &HaReceiver::onReplyFinished);
    connect(&m_importWatcher, &QFutureWatcherBase::finished,
            this, &HaReceiver::onImportWorkerFinished);

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
        addLog("[HA] URLが未入力です");
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
    addLog(QString("[HA] API: %1").arg(statesUrl().toString()));
    addLog(QString("[HA] Token: %1文字").arg(m_accessToken.size()));
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
    if (m_serverUrl.isEmpty() || m_accessToken.isEmpty()) {
        addLog("[HA] API取得をスキップ: URLまたはトークンが未設定");
        return;
    }

    QNetworkRequest request(statesUrl());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    addLog(QString("[HA] GET %1").arg(request.url().toString()));
    QNetworkReply *reply = m_network.get(request);
    reply->setProperty("haRequestType", "poll");
}

void HaReceiver::onReplyFinished(QNetworkReply *reply) {
    reply->deleteLater();

    const QString requestType = reply->property("haRequestType").toString();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    addLog(QString("[HA] 応答: HTTP %1, %2 bytes").arg(httpStatus).arg(payload.size()));

    if (reply->error() != QNetworkReply::NoError) {
        // cancelImport() による abort は静かに無視する
        if (m_cancelImport && reply->error() == QNetworkReply::OperationCanceledError)
            return;
        const QString message = reply->errorString();
        addLog(QString("[HA] エラー: %1 (%2)").arg(message).arg(static_cast<int>(reply->error())));
        if (!payload.isEmpty())
            addLog("[HA] 応答本文: " + QString::fromUtf8(payload.left(240)));
        if (requestType.startsWith("import")) {
            m_importing = false;
            emit importingChanged();
        } else {
            setConnected(false);
            setStatusText("エラー: " + message);
        }
        return;
    }

    if (requestType == "importStates") {
        // キャンセル済みなら無視
        if (!m_cancelImport)
            processImportStates(payload);
    } else if (requestType == "importHistory") {
        // キャンセル済みなら無視（遅延レスポンスが返ってきても再取得しない）
        if (!m_cancelImport)
            processImportHistory(payload, reply->property("haEntityId").toString());
    } else {
        processStates(payload);
        if (!m_connected) {
            addLog("[HA] Home Assistant 接続確認");
            setConnected(true);
        }
        setStatusText(QString("接続済み: %1").arg(m_serverUrl));
    }
}

void HaReceiver::processStates(const QByteArray &payload) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        addLog(QString("[HA] JSON解析エラー: %1 at offset %2")
                   .arg(parseError.errorString())
                   .arg(parseError.offset));
        addLog("[HA] 応答先頭: " + QString::fromUtf8(payload.left(240)));
        return;
    }
    if (!doc.isArray()) {
        addLog("[HA] /api/states の応答をJSON配列として読めませんでした");
        return;
    }

    addLog(QString("[HA] state数: %1").arg(doc.array().size()));
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

    addLog(QString("[HA] Bermuda Area候補: %1").arg(matched));
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

void HaReceiver::importHistory(int days) {
    if (m_importing) return;
    if (m_serverUrl.isEmpty()) {
        addLog("[HA] インポート不可: URLが未設定");
        return;
    }
    if (m_accessToken.isEmpty()) {
        addLog("[HA] インポート不可: トークンが未設定");
        return;
    }

    m_importing = true;
    m_importDays = days;
    m_importEntityMac.clear();
    m_importEntityName.clear();
    emit importingChanged();
    addLog(QString("[HA] 過去%1日分の履歴インポート開始").arg(days));

    QNetworkRequest request(statesUrl());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    m_importReply = m_network.get(request);
    m_importReply->setProperty("haRequestType", "importStates");
}

void HaReceiver::processImportStates(const QByteArray &payload) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        addLog("[HA] インポート: states解析エラー: " + parseError.errorString());
        m_importing = false;
        emit importingChanged();
        return;
    }

    m_importEntityMac.clear();
    m_importEntityName.clear();
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject obj = v.toObject();
        const QString entityId = obj.value("entity_id").toString();
        if (!entityId.startsWith("sensor.")) continue;

        const QJsonObject attrs = obj.value("attributes").toObject();
        const QString currentMac   = attrs.value("current_mac").toString().trimmed();
        const QString areaName     = attrs.value("area_name").toString().trimmed();
        const QString friendlyName = attrs.value("friendly_name").toString().trimmed();
        const QString deviceClass  = attrs.value("device_class").toString().trimmed();

        if (currentMac.isEmpty() || areaName.isEmpty()
            || !friendlyName.endsWith(" Area", Qt::CaseInsensitive)
            || (!deviceClass.isEmpty() && deviceClass != "bermuda__custom_device_class"))
            continue;

        m_importEntityMac[entityId]  = currentMac;
        m_importEntityName[entityId] = friendlyDeviceName(friendlyName, entityId);
    }

    if (m_importEntityMac.isEmpty()) {
        addLog("[HA] インポート: BermudaのAreaセンサーが見つかりません");
        m_importing = false;
        emit importingChanged();
        return;
    }

    addLog(QString("[HA] インポート: %1件のデバイスを検出").arg(m_importEntityMac.size()));

    // エンティティ × 日数分のチャンクを生成（1チャンク = 1日分）
    const QDateTime now = QDateTime::currentDateTime().toUTC();
    m_importChunks.clear();
    for (const QString &entityId : m_importEntityMac.keys()) {
        for (int day = m_importDays; day >= 1; --day) {
            ImportChunk chunk;
            chunk.entityId = entityId;
            chunk.fromIso  = now.addDays(-day).toString(Qt::ISODate);
            chunk.toIso    = (day == 1) ? now.toString(Qt::ISODate)
                                        : now.addDays(-(day - 1)).toString(Qt::ISODate);
            m_importChunks.append(chunk);
        }
    }
    m_importTotalChunks = m_importChunks.size();
    m_importedCount = 0;
    addLog(QString("[HA] インポート: %1チャンク (1日×%2エンティティ×%3日)")
               .arg(m_importTotalChunks).arg(m_importEntityMac.size()).arg(m_importDays));
    fetchNextEntityHistory();
}

void HaReceiver::cancelImport() {
    if (!m_importing) return;
    m_cancelImport = true;
    if (m_cancelFlag) m_cancelFlag->store(true);
    m_importChunks.clear();
    // ダウンロード中のHTTP replyを即座に中断してデータ転送を止める
    if (m_importReply) {
        m_importReply->abort();
        m_importReply = nullptr;
    }
    addLog(QString("[HA] キャンセル (処理済み: %1件)").arg(m_importedCount));
    m_importing = false;
    emit importingChanged();
}

void HaReceiver::fetchNextEntityHistory() {
    if (m_importChunks.isEmpty()) {
        addLog(QString("[HA] インポート完了: 計%1件").arg(m_importedCount));
        if (m_importedCount == 0)
            addLog("[HA] データなし: HAのLogbookにこのセンサーの履歴がない可能性があります");
        m_cancelImport = false;
        m_importing = false;
        emit importingChanged();
        return;
    }

    const ImportChunk chunk = m_importChunks.takeFirst();
    m_importingEntityId = chunk.entityId;

    const int done = m_importTotalChunks - m_importChunks.size();
    addLog(QString("[HA] インポート (%1/%2): %3 %4")
               .arg(done).arg(m_importTotalChunks)
               .arg(chunk.entityId)
               .arg(chunk.fromIso.left(10)));

    // History API は actual state changes のみ返す（Logbookは全更新を返すため遅い）
    // significant_changes_only のデフォルトは true = 部屋変化のみ
    const QByteArray rawUrl =
        (trimmedUrl(m_serverUrl)
         + "/api/history/period/" + chunk.fromIso
         + "?filter_entity_id=" + chunk.entityId
         + "&end_time=" + chunk.toIso
         + "&minimal_response=true").toUtf8();

    const QUrl url = QUrl::fromEncoded(rawUrl, QUrl::TolerantMode);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    request.setRawHeader("Accept", "application/json");
    m_importReply = m_network.get(request);
    m_importReply->setProperty("haRequestType", "importHistory");
    m_importReply->setProperty("haEntityId", chunk.entityId);
}

void HaReceiver::processImportHistory(const QByteArray &payload, const QString &entityId) {
    m_importingEntityId = entityId;
    m_cancelFlag = std::make_shared<std::atomic<bool>>(false);
    auto cancelFlag = m_cancelFlag;

    addLog("[HA] インポート: バックグラウンドで処理中...");

    // QtConcurrent::run + QFutureWatcher でスレッドセーフにバックグラウンド処理する。
    // 完了時に QFutureWatcher::finished シグナルがメインスレッドで発火する。
    m_importWatcher.setFuture(
        QtConcurrent::run([payload, cancelFlag]() -> QList<QPair<qint64, QString>> {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isArray())
                return {};

            // History API レスポンス: [[{state, last_changed, ...}, ...], ...]
            // significant_changes_only=true(デフォルト)で実際の状態変化のみ返る
            QList<QPair<qint64, QString>> results;
            QString prevRoom;
            for (const QJsonValue &entityVal : doc.array()) {
                if (!entityVal.isArray()) continue;
                for (const QJsonValue &v : entityVal.toArray()) {
                    if (cancelFlag->load()) break;

                    const QJsonObject entry = v.toObject();
                    const QString room = entry.value("state").toString().trimmed();
                    if (room.isEmpty() || room == "unknown" || room == "unavailable") continue;
                    if (room == prevRoom) continue;
                    prevRoom = room;

                    const QDateTime dt = QDateTime::fromString(
                        entry.value("last_changed").toString(), Qt::ISODateWithMs);
                    if (!dt.isValid()) continue;

                    results.append({dt.toSecsSinceEpoch(), room});
                }
            }
            return results;
        })
    );
}

void HaReceiver::onImportWorkerFinished() {
    // キャンセル済みなら結果を破棄。UIはcancelImport()で既に更新済み
    if (m_cancelImport) {
        m_cancelImport = false;
        return;
    }

    const QList<QPair<qint64, QString>> results = m_importWatcher.result();
    const QString entityId   = m_importingEntityId;
    const QString mac        = m_importEntityMac.value(entityId);
    const QString deviceName = m_importEntityName.value(entityId);

    for (const auto &[timestamp, room] : results) {
        emit historicalRecordReady(mac, deviceName, room, timestamp);
        ++m_importedCount;
    }

    if (!results.isEmpty())
        addLog(QString("[HA] +%1件").arg(results.size()));
    fetchNextEntityHistory();
}

