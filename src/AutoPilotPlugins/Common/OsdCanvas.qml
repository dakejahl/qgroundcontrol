pragma ComponentBehavior: Bound

import QtQuick

import QGroundControl
import QGroundControl.Controls

Rectangle {
    id: root

    implicitHeight: width * canvasHeight / canvasWidth
    color: palette.windowShadeDark
    border.color: palette.text
    clip: true
    focus: true

    required property var layout
    property int selected: -1
    property bool snap: true
    readonly property var canvas: layout ? layout.canvas : ({})
    readonly property var display: layout ? layout.display : ({})
    readonly property int cellWidth: display.cell ? display.cell.width : 12
    readonly property int cellHeight: display.cell ? display.cell.height : 18
    readonly property real canvasWidth: Math.max(1, canvas.width || (canvas.cols || 30) * cellWidth)
    readonly property real canvasHeight: Math.max(1, canvas.height || (canvas.rows || 13) * cellHeight)
    readonly property real scaleFactor: width / canvasWidth

    signal elementSelected(int index)

    function nudge(dx, dy) {
        if (!layout || layout.locked || selected < 0 || selected >= layout.elements.length) {
            return
        }
        const element = layout.elements[selected]
        const grid = display.model === "grid" || snap
        layout.moveElement(selected, element.left + dx * (grid ? cellWidth : 1),
                           element.top + dy * (grid ? cellHeight : 1), grid)
    }

    Keys.onLeftPressed: nudge(-1, 0)
    Keys.onRightPressed: nudge(1, 0)
    Keys.onUpPressed: nudge(0, -1)
    Keys.onDownPressed: nudge(0, 1)
    Keys.onDeletePressed: {
        if (layout && !layout.locked) {
            layout.removeElement(selected)
            elementSelected(-1)
        }
    }
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_H && layout && selected >= 0 && selected < layout.elements.length) {
            layout.updateElement(selected, {visible: !layout.elements[selected].shown})
            event.accepted = true
        }
    }

    QGCPalette { id: palette; colorGroupEnabled: root.enabled }

    Repeater {
        model: root.canvas.cols || 0
        Rectangle {
            required property int index
            x: index * root.cellWidth * root.scaleFactor
            width: 1
            height: root.height
            color: palette.text
            opacity: 0.12
        }
    }
    Repeater {
        model: root.canvas.rows || 0
        Rectangle {
            required property int index
            y: index * root.cellHeight * root.scaleFactor
            height: 1
            width: root.width
            color: palette.text
            opacity: 0.12
        }
    }

    Repeater {
        model: root.layout ? root.layout.elements : []

        Rectangle {
            id: elementItem
            required property int index
            required property var modelData

            x: modelData.left * root.scaleFactor
            y: modelData.top * root.scaleFactor
            width: Math.max(root.cellWidth, modelData.catalog.width * root.cellWidth) * root.scaleFactor
            height: Math.max(root.cellHeight, modelData.catalog.height * root.cellHeight) * root.scaleFactor
            color: root.selected === index ? palette.buttonHighlight : palette.windowShade
            border.color: root.selected === index ? palette.buttonHighlightText : palette.text
            opacity: modelData.shown ? 0.9 : 0.35

            QGCLabel {
                anchors.fill: parent
                anchors.margins: 1
                text: elementItem.modelData.text || elementItem.modelData.catalog.name || elementItem.modelData.id
                font.pixelSize: Math.min(ScreenTools.defaultFontPixelHeight, parent.height * 0.6)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                color: root.selected === elementItem.index ? palette.buttonHighlightText : palette.text
            }

            MouseArea {
                anchors.fill: parent
                enabled: root.layout && !root.layout.locked
                drag.target: elementItem
                drag.minimumX: 0
                drag.minimumY: 0
                drag.maximumX: root.width - root.scaleFactor
                drag.maximumY: root.height - root.scaleFactor
                onPressed: {
                    root.elementSelected(elementItem.index)
                    root.forceActiveFocus()
                }
                onReleased: (mouse) => {
                    const index = elementItem.index
                    const x = Math.round(elementItem.x / root.scaleFactor)
                    const y = Math.round(elementItem.y / root.scaleFactor)
                    elementItem.x = Qt.binding(function() { return elementItem.modelData.left * root.scaleFactor })
                    elementItem.y = Qt.binding(function() { return elementItem.modelData.top * root.scaleFactor })
                    root.layout.moveElement(index, x, y, root.snap && !(mouse.modifiers & Qt.ShiftModifier))
                }
            }
        }
    }
}
