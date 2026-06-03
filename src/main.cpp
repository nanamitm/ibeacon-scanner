#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStyleHints>
#include <QIcon>
#include "BeaconScanner.h"
#include "MqttReceiver.h"
#include "LocationHistory.h"

int main(int argc, char *argv[]) {
    // Qt 6.5+ : システムのライト/ダークに自動追随（タイトルバー含む）
    QGuiApplication::setDesktopSettingsAware(true);
    QQuickStyle::setStyle("Windows"); // ネイティブWindowsスタイル

    QGuiApplication app(argc, argv);
    app.setOrganizationName("ibeacon-scanner");
    app.setApplicationName("ibeacon-scanner");
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));

    // Qt 6.5+ カラースキームをシステムに追随
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    app.styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
#endif

    BeaconScanner scanner;
    MqttReceiver  mqtt;
    LocationHistory history;

    // MQTTのログをBeaconScannerのログに転送
    QObject::connect(&mqtt, &MqttReceiver::logMessage,
                     &scanner, &BeaconScanner::appendLog);

    // ESPresenseから部屋情報を受け取ったら履歴に保存
    QObject::connect(&mqtt, &MqttReceiver::deviceLocationUpdated,
                     &history, &LocationHistory::addRecord);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("scanner",    &scanner);
    engine.rootContext()->setContextProperty("beaconModel", scanner.model());
    engine.rootContext()->setContextProperty("logModel",   scanner.logModel());
    engine.rootContext()->setContextProperty("mqtt",       &mqtt);
    engine.rootContext()->setContextProperty("history",    &history);
    engine.rootContext()->setContextProperty("historyResults", history.results());

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []{ QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("IBeaconScanner", "Main");
    return app.exec();
}

