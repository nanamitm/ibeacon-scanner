#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStyleHints>
#include <QIcon>
#include "BeaconScanner.h"
#include "MqttReceiver.h"
#include "HaReceiver.h"
#include "LocationHistory.h"

int main(int argc, char *argv[]) {
    QGuiApplication::setDesktopSettingsAware(true);
    QQuickStyle::setStyle("Windows");

    QGuiApplication app(argc, argv);
    app.setOrganizationName("ibeacon-scanner");
    app.setApplicationName("ibeacon-scanner");
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    app.styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
#endif

    BeaconScanner scanner;
    MqttReceiver  mqtt;
    HaReceiver    ha;
    LocationHistory history;

    QObject::connect(&mqtt, &MqttReceiver::logMessage,
                     &scanner, &BeaconScanner::appendLog);
    QObject::connect(&ha, &HaReceiver::logMessage,
                     &scanner, &BeaconScanner::appendLog);
    QObject::connect(&mqtt, &MqttReceiver::deviceLocationUpdated,
                     &history, &LocationHistory::addRecord);
    QObject::connect(&ha, &HaReceiver::deviceLocationUpdated,
                     &history, &LocationHistory::addRecord);
    QObject::connect(&ha, &HaReceiver::historicalRecordReady,
                     &history, &LocationHistory::importRecord);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("scanner",       &scanner);
    engine.rootContext()->setContextProperty("beaconModel",   scanner.model());
    engine.rootContext()->setContextProperty("logModel",      scanner.logModel());
    engine.rootContext()->setContextProperty("mqtt",          &mqtt);
    engine.rootContext()->setContextProperty("ha",            &ha);
    engine.rootContext()->setContextProperty("history",       &history);
    engine.rootContext()->setContextProperty("historyResults", history.results());

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []{ QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("IBeaconScanner", "Main");
    return app.exec();
}
