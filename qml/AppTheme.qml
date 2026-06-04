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
    readonly property color button: dark ? "#334155" : "#E8EEF7"
    readonly property color buttonPressed: dark ? "#475569" : "#D6E2F2"
    readonly property color buttonText: dark ? "#F8FAFC" : "#111827"
    readonly property color buttonBorder: dark ? "#64748B" : "#B6C3D4"
    readonly property color disabledText: dark ? "#737A86" : "#9CA3AF"
    readonly property color success: dark ? "#81C995" : "#2E7D32"
    readonly property color warning: dark ? "#FDD663" : "#F57C00"
    readonly property color danger: dark ? "#F28B82" : "#C62828"
    readonly property color neutral: dark ? "#AAB0BA" : "#757575"

    readonly property color logBackground: dark ? "#111318" : palette.base
    readonly property color logText:       dark ? "#DADCE0" : palette.windowText
    readonly property color logSuccess:    dark ? "#81C995" : "#2E7D32"
    readonly property color logWarning:    dark ? "#FDD663" : "#E65100"
    readonly property color logError:      dark ? "#F28B82" : "#C62828"

    function luminance(color) {
        return 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b
    }
}

