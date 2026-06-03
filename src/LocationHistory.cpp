#include "LocationHistory.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

// --- HistoryModel ---

HistoryModel::HistoryModel(QObject *parent) : QAbstractListModel(parent) {}

int HistoryModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_records.size();
}

QVariant HistoryModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_records.size()) return {};
    const auto &r = m_records.at(index.row());
    switch (role) {
    case TimestampRole:  return r.timestamp.toString("yyyy/MM/dd HH:mm:ss");
    case RoomRole:       return r.room;
    case DeviceNameRole: return r.deviceName;
    }
    return {};
}

QHash<int, QByteArray> HistoryModel::roleNames() const {
    return {
        {TimestampRole,  "timestamp"},
        {RoomRole,       "room"},
        {DeviceNameRole, "deviceName"},
    };
}

void HistoryModel::setRecords(const QList<HistoryRecord> &records) {
    beginResetModel();
    m_records = records;
    endResetModel();
}

// --- LocationHistory ---

LocationHistory::LocationHistory(QObject *parent) : QObject(parent) {
    initDb();
    loadKnownDevices();
}

void LocationHistory::initDb() {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(path + "/location_history.db");
    if (!m_db.open()) {
        qWarning() << "DB open failed:" << m_db.lastError().text();
        return;
    }

    QSqlQuery q(m_db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS location_history (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id   TEXT NOT NULL,
            device_name TEXT,
            room        TEXT NOT NULL,
            timestamp   INTEGER NOT NULL
        )
    )");
    q.exec("CREATE INDEX IF NOT EXISTS idx_device_ts ON location_history(device_id, timestamp)");
}

void LocationHistory::loadKnownDevices() {
    QSqlQuery q(m_db);
    q.exec("SELECT DISTINCT device_id, device_name FROM location_history ORDER BY device_name");
    QStringList list;
    while (q.next()) {
        const QString name = q.value(1).toString();
        list << (name.isEmpty() ? q.value(0).toString() : name);
    }
    if (m_knownDevices != list) {
        m_knownDevices = list;
        emit knownDevicesChanged();
    }
}

void LocationHistory::addRecord(const QString &deviceId, const QString &deviceName,
                                const QString &room) {
    if (m_lastRoom.value(deviceId) == room) return; // 同じ部屋への重複記録を防ぐ
    m_lastRoom[deviceId] = room;

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO location_history (device_id, device_name, room, timestamp) "
              "VALUES (?, ?, ?, ?)");
    q.addBindValue(deviceId);
    q.addBindValue(deviceName);
    q.addBindValue(room);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!q.exec())
        qWarning() << "Insert failed:" << q.lastError().text();

    const QString display = deviceName.isEmpty() ? deviceId : deviceName;
    if (!m_knownDevices.contains(display)) {
        m_knownDevices.append(display);
        emit knownDevicesChanged();
    }
}

void LocationHistory::search(const QString &deviceName, int rangeIndex) {
    const QDate today = QDate::currentDate();
    QDateTime from, to = QDateTime::currentDateTime();

    switch (rangeIndex) {
    case 0: from = QDateTime(today,              QTime(0, 0)); break;
    case 1: from = QDateTime(today.addDays(-1),  QTime(0, 0));
            to   = QDateTime(today,              QTime(0, 0)); break;
    case 2: from = QDateTime(today.addDays(-7),  QTime(0, 0)); break;
    default: from = QDateTime::fromSecsSinceEpoch(0); break;
    }

    QSqlQuery q(m_db);
    q.prepare(R"(
        SELECT device_name, room, timestamp
        FROM location_history
        WHERE (device_name = ? OR device_id = ?)
          AND timestamp >= ? AND timestamp <= ?
        ORDER BY timestamp DESC
        LIMIT 500
    )");
    q.addBindValue(deviceName);
    q.addBindValue(deviceName);
    q.addBindValue(from.toSecsSinceEpoch());
    q.addBindValue(to.toSecsSinceEpoch());
    q.exec();

    QList<HistoryRecord> records;
    while (q.next()) {
        records.append({
            q.value(0).toString(),
            q.value(1).toString(),
            QDateTime::fromSecsSinceEpoch(q.value(2).toLongLong()),
        });
    }
    m_results.setRecords(records);
    m_resultCount = records.size();
    emit resultCountChanged();
}
