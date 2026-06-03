#pragma once
#include <QObject>
#include <QAbstractListModel>
#include <QDateTime>
#include <QSqlDatabase>

struct HistoryRecord {
    QString deviceName;
    QString room;
    QDateTime timestamp;
};

class HistoryModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { TimestampRole = Qt::UserRole + 1, RoomRole, DeviceNameRole };

    explicit HistoryModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setRecords(const QList<HistoryRecord> &records);

private:
    QList<HistoryRecord> m_records;
};

class LocationHistory : public QObject {
    Q_OBJECT
    Q_PROPERTY(HistoryModel* results READ results CONSTANT)
    Q_PROPERTY(QStringList knownDevices READ knownDevices NOTIFY knownDevicesChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultCountChanged)

public:
    explicit LocationHistory(QObject *parent = nullptr);

    HistoryModel *results() { return &m_results; }
    QStringList knownDevices() const { return m_knownDevices; }
    int resultCount() const { return m_resultCount; }

    void addRecord(const QString &deviceId, const QString &deviceName, const QString &room);

    // rangeIndex: 0=今日 1=昨日 2=今週 3=全期間
    Q_INVOKABLE void search(const QString &deviceName, int rangeIndex);

signals:
    void knownDevicesChanged();
    void resultCountChanged();

private:
    void initDb();
    void loadKnownDevices();

    QSqlDatabase m_db;
    HistoryModel m_results;
    QStringList m_knownDevices;
    QMap<QString, QString> m_lastRoom; // deviceId → 直前の部屋（重複防止）
    int m_resultCount = 0;
};
