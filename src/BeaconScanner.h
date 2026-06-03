#pragma once
#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QAbstractListModel>
#include <QStringListModel>
#include <QTimer>
#include "BeaconInfo.h"

class BeaconModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int txPowerOffset READ txPowerOffset WRITE setTxPowerOffset NOTIFY txPowerOffsetChanged)
public:
    enum Roles {
        AddressRole = Qt::UserRole + 1,
        NameRole,
        UuidRole,
        MajorRole,
        MinorRole,
        RssiRole,
        IsIBeaconRole,
        DistanceRole,
        LastSeenRole,
    };

    explicit BeaconModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void updateOrAdd(const BeaconInfo &info);
    Q_INVOKABLE void clear();

    int txPowerOffset() const { return m_txPowerOffset; }
    void setTxPowerOffset(int offset);

signals:
    void txPowerOffsetChanged();

private:
    QString elapsedText(const QDateTime &dateTime) const;
    void refreshElapsedTimes();

    QList<BeaconInfo> m_beacons;
    int m_txPowerOffset = 0;
    QTimer m_elapsedTimer;
};

class BeaconScanner : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(BeaconModel* model READ model CONSTANT)
    Q_PROPERTY(QStringListModel* logModel READ logModel CONSTANT)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(int scanInterval READ scanInterval WRITE setScanInterval NOTIFY scanIntervalChanged)

public:
    explicit BeaconScanner(QObject *parent = nullptr);

    bool scanning() const { return m_scanning; }
    BeaconModel *model() { return &m_model; }
    QStringListModel *logModel() { return &m_logModel; }
    QString statusText() const { return m_statusText; }
    int scanInterval() const { return m_restartTimer.interval(); }
    void setScanInterval(int ms);

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE void clearLog();
    void appendLog(const QString &text);

signals:
    void scanningChanged();
    void statusTextChanged();
    void scanIntervalChanged();

private:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onErrorOccurred(QBluetoothDeviceDiscoveryAgent::Error error);
    void parseIBeacon(const QBluetoothDeviceInfo &info, BeaconInfo &beacon);
    void setStatusText(const QString &text);
    void addLog(const QString &text);
    void scheduleRestart();

    QBluetoothDeviceDiscoveryAgent m_agent;
    BeaconModel m_model;
    QStringListModel m_logModel;
    QTimer m_restartTimer;
    bool m_scanning = false;
    bool m_restarting = false;
    QString m_statusText;
};





