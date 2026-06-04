pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    readonly property var theme: appTheme
    visible: true
    width: 520
    height: 800
    title: "iBeacon Scanner"
    color: palette.window

    AppTheme {
        id: appTheme
        palette: root.palette
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // ステータスバー
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: palette.highlight
            radius: 4

            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                Text {
                    text: scanner.statusText
                    color: palette.highlightedText
                    font.pixelSize: 14
                    Layout.fillWidth: true
                }
                Item {
                    visible: scanner.scanning
                    implicitWidth: 28
                    implicitHeight: 28

                    Repeater {
                        model: 8

                        Rectangle {
                            required property int index

                            width: 4
                            height: 4
                            radius: 2
                            color: palette.highlightedText
                            opacity: 0.25 + index * 0.09
                            x: 14 + Math.cos((index / 8) * Math.PI * 2) * 9 - width / 2
                            y: 14 + Math.sin((index / 8) * Math.PI * 2) * 9 - height / 2
                        }
                    }

                    RotationAnimator on rotation {
                        running: scanner.scanning
                        loops: Animation.Infinite
                        from: 0
                        to: 360
                        duration: 900
                    }
                }
            }
        }

        // タブ切り替え
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            TabButton { text: "デバイス" }
            TabButton { text: "履歴" }
            TabButton { text: "ログ" }
            TabButton { text: "設定" }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            // --- タブ0: デバイスリスト ---
            ColumnLayout {
                spacing: 4

                Text {
                    text: listView.count + " デバイス検出"
                    font.pixelSize: 12
                    color: theme.placeholderText
                    Layout.alignment: Qt.AlignRight
                }

                ListView {
                    id: listView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 6
                    clip: true
                    model: beaconModel

                    delegate: BeaconListItem {
                        // Qt 6: required property はモデルロールから自動バインドされる
                        width: listView.width
                        theme: root.theme
                    }
                }
            }

            // --- タブ1: 履歴 ---
            HistoryTab { theme: root.theme }

            // --- タブ2: ログ ---
            ColumnLayout {
                spacing: 4

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: theme.logBackground
                    radius: 4
                    clip: true

                    ListView {
                        id: logView
                        anchors { fill: parent; margins: 4 }
                        model: logModel
                        spacing: 1

                        onCountChanged: Qt.callLater(() => positionViewAtEnd())

                        delegate: Text {
                            required property string display

                            width: logView.width
                            text: display
                            font.pixelSize: 10
                            font.family: "Courier New"
                            wrapMode: Text.WrapAnywhere
                            color: {
                                if (display.includes("⚠") || display.includes("Error"))
                                    return root.theme.logError
                                if (display.includes("iBeacon!"))
                                    return root.theme.logSuccess
                                if (display.includes("[Init]"))
                                    return root.theme.logWarning
                                return root.theme.logText
                            }
                        }
                    }
                }

                Button {
                    text: "ログクリア"
                    Layout.alignment: Qt.AlignRight
                    onClicked: scanner.clearLog()
                }
            }

            // --- タブ3: 設定 ---
            ScrollView {
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: 16

                // TX Power 補正
                Text {
                    text: "TX Power 補正オフセット"
                    font.pixelSize: 14
                    font.bold: true
                    color: theme.bodyText
                }

                GroupBox {
                    title: ""
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Text {
                            text: "ビーコンのTX Power校正値がズレている場合に補正します。\n"
                                + "距離が実際より遠く出る場合は負の値、近く出る場合は正の値を設定します。"
                            font.pixelSize: 12
                            color: theme.bodyText
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text {
                                text: "補正値:"
                                font.pixelSize: 13
                                color: theme.bodyText
                            }

                            Slider {
                                id: offsetSlider
                                from: -30
                                to: 30
                                stepSize: 1
                                value: beaconModel.txPowerOffset
                                Layout.fillWidth: true
                                onMoved: beaconModel.txPowerOffset = value
                            }

                            Text {
                                text: (beaconModel.txPowerOffset >= 0 ? "+" : "")
                                      + beaconModel.txPowerOffset + " dBm"
                                font.pixelSize: 13
                                font.bold: true
                                color: beaconModel.txPowerOffset === 0 ? theme.neutral : theme.accentText
                                Layout.preferredWidth: 60
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: tipText.implicitHeight + 16
                            color: Qt.alpha(theme.accent, theme.dark ? 0.22 : 0.14)
                            radius: 4
                            visible: beaconModel.txPowerOffset !== 0

                            Text {
                                id: tipText
                                anchors { fill: parent; margins: 8 }
                                text: "TX Power " + beaconModel.txPowerOffset
                                      + " dBm 補正中。実際の距離と合うように調整してください。"
                                font.pixelSize: 11
                                color: theme.bodyText
                                wrapMode: Text.Wrap
                            }
                        }

                        Button {
                            text: "リセット (0 dBm)"
                            enabled: beaconModel.txPowerOffset !== 0
                            onClicked: {
                                beaconModel.txPowerOffset = 0
                                offsetSlider.value = 0
                            }
                        }
                    }
                }

                // MQTT接続設定
                Text {
                    text: "MQTTブローカー (ESPresense)"
                    font.pixelSize: 14
                    font.bold: true
                    color: theme.bodyText
                }

                GroupBox {
                    title: ""
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "URL:"; font.pixelSize: 13; color: theme.bodyText; Layout.preferredWidth: 70 }
                            TextField {
                                text: mqtt.brokerUrl
                                Layout.fillWidth: true
                                placeholderText: "mqtt://192.168.1.x:1883"
                                onEditingFinished: mqtt.brokerUrl = text
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "ユーザー:"; font.pixelSize: 13; color: theme.bodyText; Layout.preferredWidth: 70 }
                            TextField {
                                text: mqtt.username
                                Layout.fillWidth: true
                                placeholderText: "(任意)"
                                onEditingFinished: mqtt.username = text
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "パスワード:"; font.pixelSize: 13; color: theme.bodyText; Layout.preferredWidth: 70 }
                            TextField {
                                text: mqtt.password
                                Layout.fillWidth: true
                                placeholderText: "(任意)"
                                echoMode: TextInput.Password
                                onEditingFinished: mqtt.password = text
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Rectangle {
                                Layout.preferredWidth: 10; Layout.preferredHeight: 10; radius: 5
                                color: mqtt.connected ? theme.success : theme.neutral
                            }
                            Text {
                                text: mqtt.statusText
                                font.pixelSize: 12
                                color: theme.bodyText
                                Layout.fillWidth: true
                                Layout.minimumWidth: 80
                                elide: Text.ElideRight
                            }
                            Button {
                                text: mqtt.connected ? "切断" : "接続"
                                Layout.preferredWidth: 88
                                Layout.minimumWidth: 88
                                onClicked: mqtt.connected ? mqtt.disconnectBroker()
                                                          : mqtt.connectBroker()
                            }
                        }

                        RowLayout {
                            spacing: 6

                            CheckBox {
                                id: autoConnectCheck
                                text: ""
                                checked: mqtt.autoConnect
                                onToggled: mqtt.autoConnect = checked
                            }

                            Text {
                                text: "起動時に自動接続する"
                                color: theme.bodyText
                                opacity: autoConnectCheck.enabled ? 1.0 : 0.45
                                verticalAlignment: Text.AlignVCenter

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: autoConnectCheck.toggle()
                                }
                            }
                        }
                    }
                }


                // Home Assistant / Bermuda接続設定
                Text {
                    text: "Home Assistant / Bermuda"
                    font.pixelSize: 14
                    font.bold: true
                    color: theme.bodyText
                }

                GroupBox {
                    title: ""
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Text {
                            text: "Bermudaが作成するAreaセンサーをHome Assistant APIから読み取り、部屋移動履歴に保存します。"
                            font.pixelSize: 12
                            color: theme.bodyText
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "URL:"; font.pixelSize: 13; color: theme.bodyText; Layout.preferredWidth: 70 }
                            TextField {
                                text: ha.serverUrl
                                Layout.fillWidth: true
                                placeholderText: "http://homeassistant.local:8123"
                                onEditingFinished: ha.serverUrl = text
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "トークン:"; font.pixelSize: 13; color: theme.bodyText; Layout.preferredWidth: 70 }
                            TextField {
                                text: ha.accessToken
                                Layout.fillWidth: true
                                placeholderText: "Long-Lived Access Token"
                                echoMode: TextInput.Password
                                onEditingFinished: ha.accessToken = text
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "更新間隔:"; font.pixelSize: 13; color: theme.bodyText; Layout.preferredWidth: 70 }
                            Slider {
                                id: haPollSlider
                                from: 5
                                to: 60
                                stepSize: 5
                                value: ha.pollInterval / 1000
                                Layout.fillWidth: true
                                onMoved: ha.pollInterval = value * 1000
                            }
                            Text {
                                text: ha.pollInterval / 1000 + " 秒"
                                font.pixelSize: 13
                                font.bold: true
                                color: theme.accentText
                                Layout.preferredWidth: 44
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Rectangle {
                                Layout.preferredWidth: 10; Layout.preferredHeight: 10; radius: 5
                                color: ha.connected ? theme.success : theme.neutral
                            }
                            Text {
                                text: "状態: " + ha.statusText
                                font.pixelSize: 12
                                color: theme.bodyText
                                Layout.fillWidth: true
                                Layout.minimumWidth: 80
                                elide: Text.ElideRight
                            }
                            Button {
                                text: ha.connected ? "停止" : "接続"
                                Layout.preferredWidth: 88
                                Layout.minimumWidth: 88
                                onClicked: ha.connected ? ha.disconnectServer()
                                                        : ha.connectServer()
                            }
                        }

                        RowLayout {
                            spacing: 6

                            CheckBox {
                                id: haAutoConnectCheck
                                text: ""
                                checked: ha.autoConnect
                                onToggled: ha.autoConnect = checked
                            }

                            Text {
                                text: "起動時に自動接続する"
                                color: theme.bodyText
                                opacity: haAutoConnectCheck.enabled ? 1.0 : 0.45
                                verticalAlignment: Text.AlignVCenter

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: haAutoConnectCheck.toggle()
                                }
                            }
                        }
                    }
                }
                // スキャン間隔
                Text {
                    text: "スキャン更新間隔"
                    font.pixelSize: 14
                    font.bold: true
                    color: theme.bodyText
                }

                GroupBox {
                    title: ""
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Text {
                            text: "短いほどRSSI・距離の更新が速くなりますが、消費電力が増えます。"
                            font.pixelSize: 12
                            color: theme.bodyText
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text {
                                text: "間隔:"
                                font.pixelSize: 13
                                color: theme.bodyText
                            }

                            Slider {
                                id: intervalSlider
                                from: 5
                                to: 30
                                stepSize: 1
                                value: scanner.scanInterval / 1000
                                Layout.fillWidth: true
                                onMoved: scanner.scanInterval = value * 1000
                            }

                            Text {
                                text: scanner.scanInterval / 1000 + " 秒"
                                font.pixelSize: 13
                                font.bold: true
                                color: theme.accentText
                                Layout.preferredWidth: 44
                            }
                        }
                    }
                }

                    Item { Layout.preferredHeight: 1 }
                }
            }
        }

        // ボタン
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: scanner.scanning ? "停止" : "スキャン開始"
                Layout.fillWidth: true
                onClicked: scanner.scanning ? scanner.stopScan() : scanner.startScan()
            }

            Button {
                text: "クリア"
                enabled: !scanner.scanning
                onClicked: beaconModel.clear()
            }
        }
    }
}
