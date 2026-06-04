#include "BeaconScanner.h"
#include <QBluetoothDeviceInfo>
#include <QBluetoothLocalDevice>
#include <QCoreApplication>
#include <QDateTime>
#include <QPermissions>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif
#include <QtEndian>
#include <QDebug>
#include <QSettings>
#include <cmath>

// --- BeaconModel ---

BeaconModel::BeaconModel(QObject *parent) : QAbstractListModel(parent) {
    QSettings s;
    m_txPowerOffset  = s.value("beacon/txPowerOffset",  0).toInt();
    m_sortKey        = s.value("beacon/sortKey",        SortByLastSeen).toInt();
    m_sortDescending = s.value("beacon/sortDescending", false).toBool();
    m_elapsedTimer.setInterval(1000);
    connect(&m_elapsedTimer, &QTimer::timeout, this, &BeaconModel::refreshElapsedTimes);
    m_elapsedTimer.start();
}

int BeaconModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_beacons.size();
}

QVariant BeaconModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_beacons.size())
        return {};
    const auto &b = m_beacons.at(index.row());
    switch (role) {
    case AddressRole:   return b.address;
    case NameRole:      return b.name;
    case UuidRole:      return b.uuid;
    case MajorRole:     return b.major;
    case MinorRole:     return b.minor;
    case RssiRole:      return b.rssi;
    case IsIBeaconRole: return b.isIBeacon();
    case DistanceRole:  return b.estimatedDistance(m_txPowerOffset);
    case LastSeenRole:  return elapsedText(b.lastSeen);
    }
    return {};
}

QHash<int, QByteArray> BeaconModel::roleNames() const {
    return {
        {AddressRole,   "address"},
        {NameRole,      "deviceName"},
        {UuidRole,      "uuid"},
        {MajorRole,     "major"},
        {MinorRole,     "minor"},
        {RssiRole,      "rssi"},
        {IsIBeaconRole, "isIBeacon"},
        {DistanceRole,  "distance"},
        {LastSeenRole,  "lastSeen"},
    };
}

void BeaconModel::updateOrAdd(const BeaconInfo &info) {
    for (int i = 0; i < m_beacons.size(); ++i) {
        if (m_beacons[i].address == info.address) {
            m_beacons[i] = info;
            emit dataChanged(index(i), index(i));
            scheduleSortIfNeeded();
            return;
        }
    }
    beginInsertRows({}, m_beacons.size(), m_beacons.size());
    m_beacons.append(info);
    endInsertRows();
    scheduleSortIfNeeded();
}

void BeaconModel::scheduleSortIfNeeded() {
    if (m_sortPending || m_beacons.size() < 2) return;
    m_sortPending = true;
    QTimer::singleShot(0, this, &BeaconModel::sortBeacons);
}

void BeaconModel::sortBeacons() {
    m_sortPending = false;
    if (m_beacons.size() < 2) return;
    beginResetModel();
    switch (m_sortKey) {
    case SortByName:
        std::sort(m_beacons.begin(), m_beacons.end(), [](const BeaconInfo &a, const BeaconInfo &b) {
            if (a.name.isEmpty() && b.name.isEmpty()) return false;
            if (a.name.isEmpty()) return false;
            if (b.name.isEmpty()) return true;
            return a.name.toLower() < b.name.toLower();
        });
        break;
    case SortByLastSeen:
        std::sort(m_beacons.begin(), m_beacons.end(), [](const BeaconInfo &a, const BeaconInfo &b) {
            return a.lastSeen > b.lastSeen; // 最新が先頭
        });
        break;
    case SortByDistance:
        std::sort(m_beacons.begin(), m_beacons.end(), [this](const BeaconInfo &a, const BeaconInfo &b) {
            const double da = a.estimatedDistance(m_txPowerOffset);
            const double db = b.estimatedDistance(m_txPowerOffset);
            if (da < 0) return false;
            if (db < 0) return true;
            return da < db;
        });
        break;
    case SortByRssi:
        // 信号が強い順（RSSIが大きい＝0に近い順）
        std::sort(m_beacons.begin(), m_beacons.end(), [](const BeaconInfo &a, const BeaconInfo &b) {
            return a.rssi > b.rssi;
        });
        break;
    case SortByType:
        // iBeacon先頭、同種内はアドレス順
        std::sort(m_beacons.begin(), m_beacons.end(), [](const BeaconInfo &a, const BeaconInfo &b) {
            if (a.isIBeacon() != b.isIBeacon()) return a.isIBeacon();
            return a.address < b.address;
        });
        break;
    case SortByAddress:
        std::sort(m_beacons.begin(), m_beacons.end(), [](const BeaconInfo &a, const BeaconInfo &b) {
            return a.address < b.address;
        });
        break;
    case SortByUuid:
        // UUID昇順、非iBeaconは末尾
        std::sort(m_beacons.begin(), m_beacons.end(), [](const BeaconInfo &a, const BeaconInfo &b) {
            if (a.uuid.isEmpty() && b.uuid.isEmpty()) return false;
            if (a.uuid.isEmpty()) return false;
            if (b.uuid.isEmpty()) return true;
            return a.uuid < b.uuid;
        });
        break;
    }
    if (m_sortDescending)
        std::reverse(m_beacons.begin(), m_beacons.end());
    endResetModel();
}

void BeaconModel::setSortDescending(bool desc) {
    if (m_sortDescending == desc) return;
    m_sortDescending = desc;
    QSettings().setValue("beacon/sortDescending", desc);
    emit sortDescendingChanged();
    sortBeacons();
}

void BeaconModel::setSortKey(int key) {
    if (m_sortKey == key) return;
    m_sortKey = key;
    QSettings().setValue("beacon/sortKey", key);
    emit sortKeyChanged();
    sortBeacons();
}

void BeaconModel::clear() {
    beginResetModel();
    m_beacons.clear();
    endResetModel();
}

QString BeaconModel::elapsedText(const QDateTime &dateTime) const {
    if (!dateTime.isValid())
        return {};

    const qint64 seconds = qMax<qint64>(0, dateTime.secsTo(QDateTime::currentDateTime()));
    if (seconds < 3)
        return "たった今";
    if (seconds < 60)
        return QString("%1秒前").arg(seconds);

    const qint64 minutes = seconds / 60;
    if (minutes < 60)
        return QString("%1分前").arg(minutes);

    const qint64 hours = minutes / 60;
    if (hours < 24)
        return QString("%1時間前").arg(hours);

    return QString("%1日前").arg(hours / 24);
}

void BeaconModel::refreshElapsedTimes() {
    if (m_beacons.isEmpty())
        return;
    emit dataChanged(index(0), index(m_beacons.size() - 1), {LastSeenRole});
}

void BeaconModel::setTxPowerOffset(int offset) {
    if (m_txPowerOffset == offset) return;
    m_txPowerOffset = offset;
    QSettings().setValue("beacon/txPowerOffset", offset);
    emit txPowerOffsetChanged();
    if (!m_beacons.isEmpty()) {
        emit dataChanged(index(0), index(m_beacons.size() - 1), {DistanceRole});
        if (m_sortKey == SortByDistance)
            sortBeacons();
    }
}

// --- BeaconScanner ---

BeaconScanner::BeaconScanner(QObject *parent) : QObject(parent) {
    m_restartTimer.setInterval(30000);
    m_agent.setLowEnergyDiscoveryTimeout(m_restartTimer.interval());

    // allDevices() はブロッキング呼び出しのためイベントループ開始後に実行
    QTimer::singleShot(0, this, [this] {
        QBluetoothLocalDevice localDevice;
        const int adapterCount = QBluetoothLocalDevice::allDevices().size();
        addLog(QString("[Init] QBluetoothLocalDevice::allDevices() = %1 件").arg(adapterCount));
        if (localDevice.isValid()) {
            addLog(QString("[Init] アダプタ名: %1").arg(localDevice.name()));
            addLog(QString("[Init] アドレス: %1").arg(localDevice.address().toString()));
            const QString state = (localDevice.hostMode() == QBluetoothLocalDevice::HostPoweredOff)
                                  ? "OFF" : "ON";
            addLog(QString("[Init] 電源状態: %1").arg(state));
        } else {
            addLog("[Init] QBluetoothLocalDevice 無効 (Windows WinRTバックエンドでは既知の動作・問題なし)");
        }
    });

    connect(&m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BeaconScanner::onDeviceDiscovered);
    connect(&m_agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, &BeaconScanner::onErrorOccurred);
    connect(&m_agent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, [this]{
                if (!m_scanning) {
                    setStatusText("スキャン完了");
                    return;
                }
                addLog("[Scan] スキャンエージェント finished シグナル受信。1秒後に再開します");
                setStatusText("スキャン待機中");
                scheduleRestart();
            });

    // Windows WinRTは強制 stop/start を短周期で繰り返すと USB アダプターが固まることがあるため、
    // LowEnergyDiscoveryTimeout の自然終了後に短い休止を挟んで再開する。
}

void BeaconScanner::startScan() {
#ifdef Q_OS_ANDROID
    // 1. Bluetooth 権限チェック
    QBluetoothPermission btPerm;
    btPerm.setCommunicationModes(QBluetoothPermission::Access);

    if (qApp->checkPermission(btPerm) == Qt::PermissionStatus::Undetermined) {
        addLog("[Scan] Android: Bluetooth 権限をリクエスト中...");
        qApp->requestPermission(btPerm, this, [this](const QPermission &p) {
            if (p.status() == Qt::PermissionStatus::Granted)
                startScan(); // 位置情報権限チェックへ進む
            else {
                addLog("[Scan] ⚠ Bluetooth 権限が拒否されました");
                addLog("[Scan]   設定 → アプリ → iBeacon Scanner → 権限 で許可してください");
                setStatusText("エラー: Bluetooth 権限が必要です");
            }
        });
        return;
    }
    if (qApp->checkPermission(btPerm) == Qt::PermissionStatus::Denied) {
        addLog("[Scan] ⚠ Bluetooth 権限が拒否されています");
        addLog("[Scan]   設定 → アプリ → iBeacon Scanner → 権限 で許可してください");
        setStatusText("エラー: Bluetooth 権限が必要です");
        return;
    }

    // 2. 位置情報権限チェック（Android 12+ でデバイス名取得に必要）
    QLocationPermission locPerm;
    locPerm.setAccuracy(QLocationPermission::Precise);
    locPerm.setAvailability(QLocationPermission::WhenInUse);

    if (qApp->checkPermission(locPerm) == Qt::PermissionStatus::Undetermined) {
        addLog("[Scan] Android: 位置情報権限をリクエスト中 (デバイス名の取得に必要)...");
        qApp->requestPermission(locPerm, this, [this](const QPermission &p) {
            if (p.status() != Qt::PermissionStatus::Granted)
                addLog("[Scan] ⚠ 位置情報権限なし: デバイス名が表示されない場合があります");
            doStartScan();
        });
        return;
    }
    if (qApp->checkPermission(locPerm) == Qt::PermissionStatus::Denied)
        addLog("[Scan] ⚠ 位置情報権限なし: デバイス名が表示されない場合があります");
#endif
    doStartScan();
}

void BeaconScanner::doStartScan() {
    m_restarting = false;
    m_model.clear();

#ifdef Q_OS_ANDROID
    startAndroidScan();
    return;
#endif

    if (m_agent.isActive())
        m_agent.stop();

    // サポートされているメソッドを確認してログ出力（初回のみ）
    const auto supported = QBluetoothDeviceDiscoveryAgent::supportedDiscoveryMethods();
    QStringList methodNames;
    if (supported & QBluetoothDeviceDiscoveryAgent::NoMethod)        methodNames << "NoMethod";
    if (supported & QBluetoothDeviceDiscoveryAgent::ClassicMethod)   methodNames << "Classic";
    if (supported & QBluetoothDeviceDiscoveryAgent::LowEnergyMethod) methodNames << "LowEnergy";
    addLog("[Scan] supportedDiscoveryMethods: " +
           (methodNames.isEmpty() ? "なし(0)" : methodNames.join(", ")));

    if (!(supported & QBluetoothDeviceDiscoveryAgent::LowEnergyMethod)) {
        addLog("[Scan] ⚠ LowEnergyMethod が未サポートです。Windowsのプライバシー設定を確認してください");
        setStatusText("エラー: BLE未サポート");
        return;
    }

    addLog("[Scan] スキャン開始 (LowEnergyMethod)");
    m_agent.setLowEnergyDiscoveryTimeout(m_restartTimer.interval());
    m_agent.start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    if (m_agent.error() != QBluetoothDeviceDiscoveryAgent::NoError) {
        addLog("[Scan] ⚠ start() 直後エラー: " + m_agent.errorString());
        return;
    }
    addLog("[Scan] start() 成功");
    m_scanning = true;
    emit scanningChanged();
    setStatusText(QString("スキャン中 (最大%1秒ごとに更新)...").arg(m_restartTimer.interval() / 1000));
}

void BeaconScanner::stopScan() {
    m_restarting = false;
    m_scanning = false;
    emit scanningChanged();
    addLog("[Scan] スキャン停止");
#ifdef Q_OS_ANDROID
    stopAndroidScan();
#else
    if (m_agent.isActive())
        m_agent.stop();
#endif
    setStatusText("停止");
}

void BeaconScanner::scheduleRestart() {
    if (!m_scanning || m_restarting)
        return;

    m_restarting = true;
    QTimer::singleShot(1000, this, [this] {
        if (!m_scanning) {
            m_restarting = false;
            return;
        }

        m_agent.setLowEnergyDiscoveryTimeout(m_restartTimer.interval());
        m_agent.start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
        if (m_agent.error() != QBluetoothDeviceDiscoveryAgent::NoError) {
            m_restarting = false;
            addLog("[Scan] ⚠ restart start() エラー: " + m_agent.errorString());
            return;
        }

        setStatusText(QString("スキャン中 (最大%1秒ごとに更新)...").arg(m_restartTimer.interval() / 1000));
        m_restarting = false;
    });
}

void BeaconScanner::setScanInterval(int ms) {
    const int clamped = qBound(5000, ms, 30000);
    if (m_restartTimer.interval() == clamped) return;
    m_restartTimer.setInterval(clamped);
    m_agent.setLowEnergyDiscoveryTimeout(clamped);
    emit scanIntervalChanged();
    if (m_scanning)
        setStatusText(QString("スキャン中 (最大%1秒ごとに更新)...").arg(clamped / 1000));
}

void BeaconScanner::clearLog() {
    m_logModel.setStringList({});
}

void BeaconScanner::onDeviceDiscovered(const QBluetoothDeviceInfo &info) {
    BeaconInfo beacon;
    beacon.address = info.address().toString();
    beacon.name    = info.name();
    beacon.rssi    = info.rssi();
    beacon.lastSeen = QDateTime::currentDateTime();

    // デバイス種別フラグ
    QString typeStr;
    if (info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)
        typeStr += "BLE ";
    if (info.coreConfigurations() & QBluetoothDeviceInfo::BaseRateCoreConfiguration)
        typeStr += "BR/EDR ";
    if (typeStr.isEmpty()) typeStr = "Unknown";

    addLog(QString("[Dev] %1 | %2| RSSI:%3 | %4")
           .arg(beacon.address)
           .arg(typeStr)
           .arg(beacon.rssi)
           .arg(beacon.name.isEmpty() ? "(no name)" : beacon.name));

    // 全メーカーデータキーをログ出力
    const auto keys = info.manufacturerIds();
    if (keys.isEmpty()) {
        addLog("      ManufacturerData: なし");
    } else {
        for (quint16 key : keys) {
            const QByteArray d = info.manufacturerData(key);
            addLog(QString("      ManufData[0x%1]: %2 bytes = %3")
                   .arg(key, 4, 16, QLatin1Char('0'))
                   .arg(d.size())
                   .arg(QString(d.toHex(' '))));
        }
    }

    parseIBeacon(info, beacon);
    if (beacon.isIBeacon()) {
        addLog(QString("      → iBeacon! UUID:%1 Maj:%2 Min:%3 TxPow:%4")
               .arg(beacon.uuid).arg(beacon.major).arg(beacon.minor).arg(beacon.txPower));
    }

    m_model.updateOrAdd(beacon);
}

void BeaconScanner::parseIBeacon(const QBluetoothDeviceInfo &info, BeaconInfo &beacon) {
    // Apple Company ID = 0x004C
    const QByteArray data = info.manufacturerData(0x004C);
    if (data.size() < 23) return;

    // iBeacon type=0x02, length=0x15
    if ((quint8)data[0] != 0x02 || (quint8)data[1] != 0x15) return;

    // UUID (16 bytes, big-endian)
    const quint8 *p = reinterpret_cast<const quint8 *>(data.constData()) + 2;
    beacon.uuid = QString("%1%2%3%4-%5%6-%7%8-%9%10-%11%12%13%14%15%16")
        .arg(p[0],2,16,QLatin1Char('0')).arg(p[1],2,16,QLatin1Char('0'))
        .arg(p[2],2,16,QLatin1Char('0')).arg(p[3],2,16,QLatin1Char('0'))
        .arg(p[4],2,16,QLatin1Char('0')).arg(p[5],2,16,QLatin1Char('0'))
        .arg(p[6],2,16,QLatin1Char('0')).arg(p[7],2,16,QLatin1Char('0'))
        .arg(p[8],2,16,QLatin1Char('0')).arg(p[9],2,16,QLatin1Char('0'))
        .arg(p[10],2,16,QLatin1Char('0')).arg(p[11],2,16,QLatin1Char('0'))
        .arg(p[12],2,16,QLatin1Char('0')).arg(p[13],2,16,QLatin1Char('0'))
        .arg(p[14],2,16,QLatin1Char('0')).arg(p[15],2,16,QLatin1Char('0'))
        .toUpper();

    beacon.major   = qFromBigEndian<quint16>(data.constData() + 18);
    beacon.minor   = qFromBigEndian<quint16>(data.constData() + 20);
    beacon.txPower = (qint8)data[22];
}

void BeaconScanner::onErrorOccurred(QBluetoothDeviceDiscoveryAgent::Error error) {
    addLog(QString("[Error] code=%1 : %2").arg(error).arg(m_agent.errorString()));
    m_restarting = false;
    m_scanning = false;
    emit scanningChanged();
    setStatusText("エラー: " + m_agent.errorString());
}

void BeaconScanner::setStatusText(const QString &text) {
    m_statusText = text;
    emit statusTextChanged();
}

void BeaconScanner::appendLog(const QString &text) {
    addLog(text);
}

void BeaconScanner::addLog(const QString &text) {
    const QString entry = QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + " " + text;
    qDebug().noquote() << entry;
    QStringList list = m_logModel.stringList();
    list.append(entry);
    if (list.size() > 200) list.removeFirst(); // 最大200行
    m_logModel.setStringList(list);
}

// ─────────────────────────────────────────────────────────────────────────────
// Android: BluetoothLeScanner JNI 実装
// Qt の QBluetoothDeviceDiscoveryAgent は non-connectable BLE (iBeacon 等)を
// Android で検出できないため、直接 BluetoothLeScanner を使用する
// ─────────────────────────────────────────────────────────────────────────────
#ifdef Q_OS_ANDROID

// BLE ADV データ TLV から指定 Company ID のメーカーデータを抽出
static QByteArray extractManufData(const QByteArray &record, quint16 companyId)
{
    for (int i = 0; i + 1 < record.size(); ) {
        const int length = static_cast<quint8>(record[i]);
        if (length == 0) break;
        if (i + length >= record.size()) break;
        if (static_cast<quint8>(record[i + 1]) == 0xFF && length >= 3) {
            const quint16 cid = static_cast<quint8>(record[i + 2])
                              | (static_cast<quint8>(record[i + 3]) << 8);
            if (cid == companyId)
                return record.mid(i + 4, length - 3);
        }
        i += 1 + length;
    }
    return {};
}

void BeaconScanner::startAndroidScan() {
    auto *scannerObj = new QJniObject("io/github/nanamitm/ibeaconscanner/QtBeaconScanner");
    if (!scannerObj->isValid()) {
        addLog("[Scan] ⚠ QtBeaconScanner クラスが見つかりません");
        delete scannerObj;
        return;
    }
    m_androidScanner = scannerObj;

    QJniObject context = QJniObject::callStaticMethod<QJniObject>(
        "org/qtproject/qt/android/QtNative",
        "getContext",
        "()Landroid/content/Context;");

    const bool ok = scannerObj->callMethod<jboolean>(
        "startScan",
        "(Landroid/content/Context;)Z",
        context.object());

    if (!ok) {
        addLog("[Scan] ⚠ Android BLE スキャン開始失敗");
        setStatusText("エラー: BLEスキャン開始失敗");
        delete scannerObj;
        m_androidScanner = nullptr;
        return;
    }

    addLog("[Scan] Android BLE スキャン開始 (SCAN_MODE_LOW_LATENCY, ポーリング方式)");
    m_scanning = true;
    emit scanningChanged();
    setStatusText("スキャン中 (Android BLE)...");

    connect(&m_androidPollTimer, &QTimer::timeout, this, &BeaconScanner::pollAndroidScan);
    m_androidPollTimer.start(100);
}

void BeaconScanner::stopAndroidScan() {
    m_androidPollTimer.stop();
    m_androidPollTimer.disconnect();
    if (m_androidScanner) {
        auto *obj = static_cast<QJniObject *>(m_androidScanner);
        obj->callMethod<void>("stopScan");
        delete obj;
        m_androidScanner = nullptr;
    }
}

void BeaconScanner::pollAndroidScan() {
    if (!m_androidScanner) return;
    auto *obj = static_cast<QJniObject *>(m_androidScanner);

    while (true) {
        QJniObject result = obj->callObjectMethod("poll", "()Ljava/lang/String;");
        if (!result.isValid()) break;
        const QString entry = result.toString();
        if (entry.isNull()) break;

        const QStringList parts = entry.split('\n');
        if (parts.size() < 4) continue;

        const QString &address = parts[0];
        const QString &devName = parts[1];
        const int rssi         = parts[2].toInt();
        const QByteArray record = QByteArray::fromBase64(parts[3].toLatin1());

        if (address.isEmpty() && devName == "SCAN_FAILED") {
            addLog(QString("[Scan] ⚠ Android BLE スキャンエラー: code=%1").arg(parts[2]));
            m_scanning = false;
            emit scanningChanged();
            setStatusText(QString("エラー: BLEスキャン失敗(code=%1)").arg(parts[2]));
            return;
        }
        if (address.isEmpty()) continue;
        onAndroidScanResult(address, devName, rssi, record);
    }
}

void BeaconScanner::onAndroidScanResult(const QString &address, const QString &name,
                                        int rssi, const QByteArray &scanRecord)
{
    BeaconInfo beacon;
    beacon.address  = address;
    beacon.name     = name;
    beacon.rssi     = rssi;
    beacon.lastSeen = QDateTime::currentDateTime();

    addLog(QString("[Dev] %1 | BLE | RSSI:%2 | %3")
           .arg(address).arg(rssi)
           .arg(name.isEmpty() ? "(no name)" : name));

    bool foundManuf = false;
    for (int i = 0; i + 1 < scanRecord.size(); ) {
        const int length = static_cast<quint8>(scanRecord[i]);
        if (length == 0) break;
        if (i + length >= scanRecord.size()) break;
        const int type = static_cast<quint8>(scanRecord[i + 1]);
        if (type == 0xFF && length >= 3) {
            const quint16 cid = static_cast<quint8>(scanRecord[i + 2])
                              | (static_cast<quint8>(scanRecord[i + 3]) << 8);
            const QByteArray d = scanRecord.mid(i + 2, length - 1);
            addLog(QString("      ManufData[0x%1]: %2 bytes = %3")
                   .arg(cid, 4, 16, QLatin1Char('0'))
                   .arg(d.size())
                   .arg(QString(d.toHex(' '))));
            foundManuf = true;
        }
        i += 1 + length;
    }
    if (!foundManuf)
        addLog("      ManufacturerData: なし");

    const QByteArray appleData = extractManufData(scanRecord, 0x004C);
    if (appleData.size() >= 23
        && static_cast<quint8>(appleData[0]) == 0x02
        && static_cast<quint8>(appleData[1]) == 0x15)
    {
        const quint8 *p = reinterpret_cast<const quint8 *>(appleData.constData()) + 2;
        beacon.uuid = QString("%1%2%3%4-%5%6-%7%8-%9%10-%11%12%13%14%15%16")
            .arg(p[0],2,16,QLatin1Char('0')).arg(p[1],2,16,QLatin1Char('0'))
            .arg(p[2],2,16,QLatin1Char('0')).arg(p[3],2,16,QLatin1Char('0'))
            .arg(p[4],2,16,QLatin1Char('0')).arg(p[5],2,16,QLatin1Char('0'))
            .arg(p[6],2,16,QLatin1Char('0')).arg(p[7],2,16,QLatin1Char('0'))
            .arg(p[8],2,16,QLatin1Char('0')).arg(p[9],2,16,QLatin1Char('0'))
            .arg(p[10],2,16,QLatin1Char('0')).arg(p[11],2,16,QLatin1Char('0'))
            .arg(p[12],2,16,QLatin1Char('0')).arg(p[13],2,16,QLatin1Char('0'))
            .arg(p[14],2,16,QLatin1Char('0')).arg(p[15],2,16,QLatin1Char('0'))
            .toUpper();
        beacon.major   = qFromBigEndian<quint16>(appleData.constData() + 18);
        beacon.minor   = qFromBigEndian<quint16>(appleData.constData() + 20);
        beacon.txPower = static_cast<qint8>(appleData[22]);
        addLog(QString("      → iBeacon! UUID:%1 Maj:%2 Min:%3 TxPow:%4")
               .arg(beacon.uuid).arg(beacon.major).arg(beacon.minor).arg(beacon.txPower));
    }

    m_model.updateOrAdd(beacon);
}

#endif // Q_OS_ANDROID










