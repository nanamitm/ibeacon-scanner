import QtQuick

QtObject {
    id: theme

    property var palette

    readonly property real windowLuminance: luminance(palette.window)
    readonly property bool dark: windowLuminance < 0.5

    readonly property color window: palette.window
    readonly property color text: palette.windowText
    readonly property color bodyText: palette.text
    readonly property color mutedText: dark ? "#B8BDC7" : "#6B7280"
    readonly property color placeholderText: dark ? "#939AA7" : palette.placeholderText
    readonly property color card: dark ? "#20242B" : palette.base
    readonly property color alternateCard: dark ? "#262B33" : palette.alternateBase
    readonly property color elevatedCard: dark ? "#2A3039" : "#FFFFFF"
    readonly property color border: dark ? "#3A4250" : palette.mid
    readonly property color subtleBorder: dark ? "#303744" : "#D7DCE3"
    readonly property color accent: dark ? "#8AB4F8" : "#1565C0"
    readonly property color accentSoft: dark ? "#1D3557" : "#E3F2FD"
    readonly property color accentText: dark ? "#AECBFA" : "#1565C0"
    readonly property color success: dark ? "#81C995" : "#2E7D32"
    readonly property color warning: dark ? "#FDD663" : "#F57C00"
    readonly property color danger: dark ? "#F28B82" : "#C62828"
    readonly property color neutral: dark ? "#AAB0BA" : "#757575"

    readonly property color logBackground: dark ? "#111318" : "#1E1E1E"
    readonly property color logText: dark ? "#DADCE0" : "#CCCCCC"
    readonly property color logSuccess: dark ? "#81C995" : "#69FF47"
    readonly property color logWarning: dark ? "#FDD663" : "#FFD93D"
    readonly property color logError: dark ? "#F28B82" : "#FF6B6B"

    function luminance(color) {
        return 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b
    }
}

