#pragma once
#include <QString>
#include <QDateTime>

struct BeaconInfo {
    QString address;
    QString name;
    QString uuid;
    int major = -1;
    int minor = -1;
    int rssi = 0;
    int txPower = 0;
    QDateTime lastSeen;

    bool isIBeacon() const { return !uuid.isEmpty(); }

    // RSSIと送信電力から距離を推定(meters)。offsetはTX Power補正値(dBm)
    double estimatedDistance(int txPowerOffset = 0) const {
        if (rssi == 0) return -1.0;
        // iBeaconはパケット内のTX Power、他デバイスは一般的なデフォルト値 -59 dBm を使用
        int base = (txPower != 0) ? txPower : -59;
        double corrected = base + txPowerOffset;
        if (corrected == 0) return -1.0;
        double ratio = rssi * 1.0 / corrected;
        if (ratio < 1.0) return std::pow(ratio, 10);
        return 0.89976 * std::pow(ratio, 7.7095) + 0.111;
    }
};



