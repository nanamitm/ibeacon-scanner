import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property var theme: localTheme

    AppTheme {
        id: localTheme
        palette: root.palette
    }
    required property string address
    required property string deviceName
    required property string uuid
    required property int major
    required property int minor
    required property int rssi
    required property bool isIBeacon
    required property real distance
    required property string lastSeen

    height: column.implicitHeight + 16
    color: isIBeacon ? Qt.alpha(theme.accent, theme.dark ? 0.20 : 0.12) : theme.card
    border.color: isIBeacon ? Qt.alpha(theme.accent, theme.dark ? 0.70 : 0.60) : theme.border
    border.width: 1
    radius: 6

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: isIBeacon ? "iBeacon" : "BLE"
                font.bold: true
                font.pixelSize: 11
                color: isIBeacon ? theme.accent : theme.placeholderText
                Layout.preferredWidth: 52
            }
            Text {
                text: deviceName.length > 0 ? deviceName : "(no name)"
                font.pixelSize: 14
                font.bold: true
                color: theme.bodyText
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Text {
                text: rssi + " dBm"
                font.pixelSize: 12
                // 電波強度は意味のある色分けなので維持
                color: rssi > -70 ? theme.success : rssi > -90 ? theme.warning : theme.danger
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: address
                font.pixelSize: 11
                font.family: "Courier New"
                color: theme.placeholderText
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Text {
                text: lastSeen.length > 0 ? ("最終 " + lastSeen) : ""
                font.pixelSize: 11
                color: theme.placeholderText
            }
        }

        Text {
            visible: isIBeacon
            text: "UUID: " + uuid
            font.pixelSize: 10
            font.family: "Courier New"
            color: theme.bodyText
            opacity: 0.75
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        RowLayout {
            visible: isIBeacon
            Text {
                text: "Major: " + major + "  Minor: " + minor
                font.pixelSize: 11
                color: theme.bodyText
                opacity: 0.75
            }
            Item { Layout.fillWidth: true }
            Text {
                text: distance >= 0 ? ("~" + distance.toFixed(2) + " m") : ""
                font.pixelSize: 11
                color: theme.accentText
            }
        }

        RowLayout {
            visible: !isIBeacon
            Item { Layout.fillWidth: true }
            Text {
                text: distance >= 0 ? ("~" + distance.toFixed(2) + " m (参考)") : ""
                font.pixelSize: 11
                color: theme.placeholderText
            }
        }
    }
}


