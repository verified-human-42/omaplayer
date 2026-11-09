import QtQuick
import QtQuick.Controls

// One key, drawn as the key: flat, small, and low in contrast, so a row of
// them reads as one band rather than as separate objects (as in OmaControl).
Rectangle {
    id: cap

    property string text
    property color ink: theme.foreground

    implicitWidth: Math.max(implicitHeight, keyText.implicitWidth + 10)
    implicitHeight: 18
    radius: Math.max(4, Math.min(6, theme.radius))
    color: Qt.rgba(ink.r, ink.g, ink.b, 0.11)

    Label {
        id: keyText
        anchors.centerIn: parent
        text: cap.text
        color: Qt.rgba(cap.ink.r, cap.ink.g, cap.ink.b, 0.8)
        font.pixelSize: 10
        font.bold: true
    }
}
