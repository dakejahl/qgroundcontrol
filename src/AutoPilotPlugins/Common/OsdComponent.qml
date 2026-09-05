import QtQuick

import QGroundControl
import QGroundControl.Controls

SetupPage {
    id: page
    pageComponent: editorComponent

    Component {
        id: editorComponent

        OsdEditor {
            width: page.availableWidth
            controller: vehicleComponent ? vehicleComponent.osdController : null
        }
    }
}
