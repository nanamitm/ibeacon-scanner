pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property var theme: localTheme

    AppTheme {
        id: localTheme
        palette: root.palette
    }

    spacing: 8

    // 検索条件
    Text {
        text: "検索条件"
        font.pixelSize: 14
        font.bold: true
        color: root.theme.bodyText
    }

    GroupBox {
        title: ""
        Layout.fillWidth: true

        GridLayout {
            anchors.fill: parent
            columns: 2
            columnSpacing: 8
            rowSpacing: 6

            Text { text: "デバイス:"; font.pixelSize: 13; color: root.theme.bodyText }
            ComboBox {
                id: deviceCombo
                Layout.fillWidth: true
                model: history.knownDevices
                displayText: count > 0 ? currentText : "(デバイスなし)"
            }

            Text { text: "期間:"; font.pixelSize: 13; color: root.theme.bodyText }
            ComboBox {
                id: rangeCombo
                Layout.fillWidth: true
                model: ["今日", "昨日", "今週", "全期間"]
            }
        }
    }

    Button {
        text: "検索"
        Layout.fillWidth: true
        enabled: history.knownDevices.length > 0
        onClicked: history.search(deviceCombo.currentText, rangeCombo.currentIndex)
    }

    // 件数
    Text {
        text: history.resultCount + " 件"
        font.pixelSize: 12
        color: root.theme.placeholderText
        Layout.alignment: Qt.AlignRight
        visible: history.resultCount >= 0
    }

    // 結果リスト
    ListView {
        id: resultList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 2
        model: historyResults

        delegate: Rectangle {
            id: resultDelegate
            required property int index
            required property string timestamp
            required property string room

            width: resultList.width
            height: row.implicitHeight + 12
            color: resultDelegate.index % 2 === 0 ? root.theme.card : root.theme.alternateCard
            border.color: root.theme.border
            border.width: 1
            radius: 3

            RowLayout {
                id: row
                anchors { fill: parent; leftMargin: 10; rightMargin: 10; topMargin: 6; bottomMargin: 6 }

                Text {
                    text: resultDelegate.timestamp
                    font.pixelSize: 11
                    font.family: "Courier New"
                    color: root.theme.placeholderText
                    Layout.preferredWidth: 145
                }

                Rectangle {
                    Layout.preferredWidth: 6; Layout.preferredHeight: 6; radius: 3
                    color: root.theme.accent
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: resultDelegate.room
                    font.pixelSize: 13
                    font.bold: true
                    color: root.theme.bodyText
                    Layout.fillWidth: true
                }
            }
        }

        ScrollBar.vertical: ScrollBar {}

        Text {
            anchors.centerIn: parent
            text: "検索結果なし"
            color: root.theme.placeholderText
            font.pixelSize: 14
            visible: resultList.count === 0
        }
    }
}





