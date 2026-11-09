import QtQuick
import QtQuick.Shapes

// The app mark, a terminal prompt made of a play triangle and an underscore
// cursor, drawn in the theme's colors instead of the icon's fixed greys.
Rectangle {
    id: mark

    property real size: 56
    property bool cursorOn: true
    property color ink: theme.stageForeground
    // Units of the 480-wide icon tile, so the geometry matches the SVG.
    readonly property real unit: size / 480

    width: size
    height: size
    radius: Math.max(8, Math.min(112 * unit, theme.radius * 2))
    color: Qt.rgba(ink.r, ink.g, ink.b, 0.08)
    border.width: 1
    border.color: Qt.rgba(ink.r, ink.g, ink.b, 0.12)

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: mark.ink
            strokeColor: mark.ink
            strokeWidth: 24 * mark.unit
            joinStyle: ShapePath.RoundJoin
            startX: 102 * mark.unit
            startY: 156 * mark.unit
            PathLine { x: 102 * mark.unit; y: 324 * mark.unit }
            PathLine { x: 246 * mark.unit; y: 240 * mark.unit }
            PathLine { x: 102 * mark.unit; y: 156 * mark.unit }
        }
    }

    Rectangle {
        x: 286 * mark.unit
        y: 288 * mark.unit
        width: 104 * mark.unit
        height: 48 * mark.unit
        radius: 8 * mark.unit
        color: theme.accent
        opacity: mark.cursorOn ? 1 : 0
    }
}
