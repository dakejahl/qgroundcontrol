pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls

ColumnLayout {
    id: root
    spacing: ScreenTools.defaultFontPixelHeight * 0.5

    required property var controller
    property int selected: -1
    readonly property var layout: controller ? controller.layout : null
    readonly property var display: layout ? layout.display : ({})
    readonly property var selection: layout && selected >= 0 && selected < layout.elements.length ? layout.elements[selected] : null
    readonly property bool editable: layout && !layout.locked

    function update(values) {
        if (layout && selection) {
            layout.updateElement(selected, values)
        }
    }

    function selectInitialDisplay() {
        if (controller && controller.available && controller.displayIndex < 0) {
            controller.selectDisplay(0)
        }
    }

    onControllerChanged: selectInitialDisplay()
    Component.onCompleted: selectInitialDisplay()

    Connections {
        target: root.controller
        function onMetadataChanged() { root.selectInitialDisplay() }
    }

    QGCLabel {
        Layout.fillWidth: true
        text: qsTr("Arrange the overlay elements, then save the layout to your vehicle.")
        wrapMode: Text.WordWrap
    }

    RowLayout {
        Layout.fillWidth: true
        QGCComboBox {
            id: displayBox
            objectName: "osdDisplaySelector"
            Layout.fillWidth: true
            model: root.controller ? root.controller.displays : []
            textRole: "name"
            currentIndex: root.controller ? root.controller.displayIndex : -1
            enabled: root.controller && !root.controller.busy && !root.layout.dirty
            onActivated: {
                root.selected = -1
                root.controller.selectDisplay(currentIndex)
            }
        }
        QGCLabel {
            text: !root.controller || !root.controller.enableFact ? qsTr("Driver state unavailable")
                  : root.controller.enableFact.rawValue === root.display.enable.disabled ? qsTr("Driver disabled") : qsTr("Driver enabled")
        }
        FactComboBox {
            visible: root.controller && root.controller.enableFact !== null && root.controller.enableFact.enumStrings.length > 0
            fact: root.controller ? root.controller.enableFact : null
            indexModel: false
            enabled: root.editable
        }
        FactTextField {
            visible: root.controller && root.controller.enableFact !== null && root.controller.enableFact.enumStrings.length === 0
            fact: root.controller ? root.controller.enableFact : null
            enabled: root.editable
        }
    }

    Flow {
        Layout.fillWidth: true
        spacing: ScreenTools.defaultFontPixelWidth

        QGCComboBox {
            id: canvasBox
            objectName: "osdCanvasSelector"
            model: root.display.canvases || []
            textRole: "name"
            valueRole: "id"
            currentIndex: root.layout ? indexOfValue(root.layout.canvas.id || "") : -1
            enabled: root.editable
            onActivated: root.layout.selectCanvas(currentValue)
        }
        QGCCheckBox {
            id: snapBox
            text: qsTr("Snap to cells")
            checked: true
            visible: root.display.model === "pixel"
        }
        QGCButton {
            objectName: "osdSaveButton"
            text: qsTr("Save to vehicle")
            enabled: root.controller && root.controller.canSave
            onClicked: root.controller.save()
        }
        QGCButton {
            text: qsTr("Fetch")
            enabled: root.controller && !root.controller.busy && !root.layout.dirty
            onClicked: { root.selected = -1; root.controller.fetch() }
        }
        QGCButton {
            text: qsTr("Revert")
            enabled: root.editable && root.layout.dirty
            onClicked: { root.selected = -1; root.layout.revert() }
        }
        QGCButton {
            text: qsTr("Defaults")
            enabled: root.editable
            onClicked: { root.selected = -1; root.layout.loadDefaults() }
        }
        QGCButton {
            text: qsTr("Import…")
            enabled: root.editable
            onClicked: importDialog.openForLoad()
        }
        QGCButton {
            text: qsTr("Export…")
            enabled: root.controller && root.controller.ready && !root.controller.busy
            onClicked: exportDialog.openForSave()
        }
        QGCButton {
            text: qsTr("Firmware…")
            visible: !!root.display.features && root.display.features.indexOf("firmware") >= 0
            enabled: root.editable && root.controller.enableFact !== null
            onClicked: firmwareDialog.openForLoad()
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: root.width > ScreenTools.defaultFontPixelWidth * 110 ? 3 : 1
        columnSpacing: ScreenTools.defaultFontPixelWidth

        ColumnLayout {
            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 24
            Layout.fillWidth: parent.columns === 1
            Layout.alignment: Qt.AlignTop
            QGCLabel { text: qsTr("Elements") }
            QGCComboBox {
                id: paletteBox
                objectName: "osdPalette"
                Layout.fillWidth: true
                model: root.display.elements || []
                textRole: "name"
            }
            QGCButton {
                objectName: "osdAddButton"
                text: qsTr("Add element")
                enabled: root.editable && paletteBox.currentIndex >= 0
                onClicked: root.selected = root.layout.addElement(root.display.elements[paletteBox.currentIndex].id)
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 14
                clip: true
                ColumnLayout {
                    width: parent.width
                    Repeater {
                        model: root.layout ? root.layout.elements : []
                        QGCButton {
                            required property int index
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.catalog.name || modelData.id
                            checkable: true
                            checked: root.selected === index
                            onClicked: root.selected = index
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            QGCLabel { text: qsTr("Layout preview · positions and footprints") }
            OsdCanvas {
                id: canvas
                objectName: "osdCanvas"
                Layout.fillWidth: true
                Layout.preferredHeight: width * canvasHeight / canvasWidth
                layout: root.layout
                selected: root.selected
                snap: snapBox.checked
                onElementSelected: (index) => root.selected = index
            }
            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Drag to move. Arrow keys nudge; H hides; Delete removes. Shift-drag bypasses pixel snapping.")
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 28
            Layout.fillWidth: parent.columns === 1
            Layout.alignment: Qt.AlignTop
            enabled: root.editable && root.selection !== null
            QGCLabel { text: root.selection ? root.selection.catalog.name || root.selection.id : qsTr("Select an element") }
            RowLayout {
                QGCLabel { text: root.display.model === "grid" ? qsTr("Column") : qsTr("X") }
                QGCTextField {
                    objectName: "osdXField"
                    Layout.fillWidth: true
                    text: root.selection ? (root.display.model === "grid" ? root.selection.col : root.selection.left) : ""
                    validator: IntValidator { bottom: 0; top: 32767 }
                    onEditingFinished: {
                        if (acceptableInput && root.selection) {
                            const x = Number(text) * (root.display.model === "grid" ? root.display.cell.width : 1)
                            root.layout.moveElement(root.selected, x, root.selection.top, false)
                        }
                    }
                }
                QGCLabel { text: root.display.model === "grid" ? qsTr("Row") : qsTr("Y") }
                QGCTextField {
                    Layout.fillWidth: true
                    text: root.selection ? (root.display.model === "grid" ? root.selection.row : root.selection.top) : ""
                    validator: IntValidator { bottom: 0; top: 32767 }
                    onEditingFinished: {
                        if (acceptableInput && root.selection) {
                            const y = Number(text) * (root.display.model === "grid" ? root.display.cell.height : 1)
                            root.layout.moveElement(root.selected, root.selection.left, y, false)
                        }
                    }
                }
            }
            QGCCheckBox {
                text: qsTr("Visible")
                checked: root.selection ? root.selection.shown : false
                onClicked: root.update({visible: checked})
            }
            QGCCheckBox {
                text: qsTr("Blink")
                visible: !!root.display.features && root.display.features.indexOf("blink") >= 0
                checked: root.selection ? root.selection.blink === true : false
                onClicked: root.update({blink: checked})
            }
            QGCComboBox {
                Layout.fillWidth: true
                visible: root.selection && root.selection.catalog.variants !== undefined
                model: root.selection ? root.selection.catalog.variants || [] : []
                currentIndex: root.selection ? root.selection.variant || 0 : -1
                onActivated: root.update({variant: currentIndex})
            }
            QGCTextField {
                Layout.fillWidth: true
                visible: root.selection && root.selection.id === "LABEL"
                text: root.selection ? root.selection.text || "" : ""
                maximumLength: 8
                validator: RegularExpressionValidator { regularExpression: /[ -~]{0,8}/ }
                onEditingFinished: if (acceptableInput) root.update({text: text})
            }
            QGCLabel {
                text: qsTr("Value channel")
                visible: channelBox.visible
            }
            QGCComboBox {
                id: channelBox
                Layout.fillWidth: true
                visible: !!root.selection && !!root.selection.catalog.bind && root.selection.catalog.bind.type === "value"
                model: root.display.channels || []
                textRole: "name"
                valueRole: "id"
                currentIndex: root.selection ? indexOfValue(root.selection.bind && root.selection.bind.channel !== undefined
                                                           ? root.selection.bind.channel : (root.selection.catalog.bind || {}).channel) : -1
                onActivated: {
                    const bind = Object.assign({}, root.selection.bind || {})
                    bind.channel = currentValue
                    root.update({bind: bind})
                }
            }
            QGCComboBox {
                Layout.fillWidth: true
                visible: channelBox.visible
                model: ["int", "f1", "f0", "deg", "mode", "arm", "f2", "latlon", "time", "sdeg", "pct"]
                currentIndex: root.selection ? model.indexOf((root.selection.bind || {}).format || (root.selection.catalog.bind || {}).format || "int") : -1
                onActivated: {
                    const bind = Object.assign({}, root.selection.bind || {})
                    bind.format = currentText
                    root.update({bind: bind})
                }
            }
            QGCTextField {
                Layout.fillWidth: true
                visible: channelBox.visible
                placeholderText: qsTr("Suffix")
                text: root.selection ? (root.selection.bind || {}).suffix !== undefined ? root.selection.bind.suffix : (root.selection.catalog.bind || {}).suffix || "" : ""
                maximumLength: 8
                validator: RegularExpressionValidator { regularExpression: /[ -~]{0,8}/ }
                onEditingFinished: {
                    if (acceptableInput && root.selection) {
                        const bind = Object.assign({}, root.selection.bind || {})
                        bind.suffix = text
                        root.update({bind: bind})
                    }
                }
            }
            QGCButton {
                text: qsTr("Remove")
                onClicked: { root.layout.removeElement(root.selected); root.selected = -1 }
            }
        }
    }

    QGCLabel {
        Layout.fillWidth: true
        visible: root.controller && root.controller.ready && root.layout.errors.length > 0
        text: root.layout ? root.layout.errors.join("\n") : ""
        wrapMode: Text.WordWrap
    }
    QGCLabel {
        Layout.fillWidth: true
        text: root.controller ? root.controller.status : qsTr("Connect a vehicle that advertises OSD metadata.")
        wrapMode: Text.WordWrap
    }
    RowLayout {
        visible: root.controller && root.controller.busy
        Layout.fillWidth: true
        ProgressBar { Layout.fillWidth: true; value: root.controller ? root.controller.progress : 0 }
        QGCButton { text: qsTr("Cancel"); onClicked: root.controller.cancel() }
    }

    QGCFileDialog {
        id: importDialog
        title: qsTr("Import OSD layout")
        nameFilters: [qsTr("OSD layout (*.json)")]
        onAcceptedForLoad: (file) => { root.selected = -1; root.controller.importLayout(file); close() }
    }
    QGCFileDialog {
        id: exportDialog
        title: qsTr("Export OSD layout")
        nameFilters: [qsTr("OSD layout (*.json)")]
        defaultSuffix: "json"
        onAcceptedForSave: (file) => { root.controller.exportLayout(file); close() }
    }
    QGCFileDialog {
        id: firmwareDialog
        title: qsTr("Upload OSD board firmware")
        nameFilters: [qsTr("OSD firmware (*.bin)")]
        onAcceptedForLoad: (file) => { root.controller.updateFirmware(file); close() }
    }
}
