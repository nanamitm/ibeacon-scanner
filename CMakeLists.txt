cmake_minimum_required(VERSION 3.16)
project(ibeacon-scanner VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS
    Core
    Concurrent
    Gui
    Quick
    QuickControls2
    Bluetooth
    Network
    Sql
)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(ibeacon-scanner
    src/main.cpp
    src/BeaconScanner.h
    src/BeaconScanner.cpp
    src/BeaconInfo.h
    src/MqttReceiver.h
    src/MqttReceiver.cpp
    src/HaReceiver.h
    src/HaReceiver.cpp
    src/LocationHistory.h
    src/LocationHistory.cpp
)

qt_add_resources(ibeacon-scanner "app_icons"
    PREFIX "/icons"
    BASE "resources"
    FILES
        resources/app_icon.png
)

if(WIN32)
    target_sources(ibeacon-scanner PRIVATE resources/app_icon.rc)
endif()
qt_add_qml_module(ibeacon-scanner
    URI IBeaconScanner
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/AppTheme.qml
        qml/BeaconListItem.qml
        qml/HistoryTab.qml
)

target_link_libraries(ibeacon-scanner PRIVATE
    Qt6::Core
    Qt6::Concurrent
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Bluetooth
    Qt6::Network
    Qt6::Sql
)

# Android固有設定
if(ANDROID)
    set_target_properties(ibeacon-scanner PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "io.github.nanamitm.ibeaconscanner"
        QT_ANDROID_APP_NAME "iBeacon Scanner"
        QT_ANDROID_APP_ICON "@drawable/app_icon"
    )
endif()

