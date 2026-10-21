import QtQuick
import "../src/tray"

Window {
    id: window

    width: 800
    height: 600
    visible: true
    title: "Syrtis"

    Tray {
        id: tray
        objectName: "systemTray"
        window: window
    }

    onClosing: function(close) {
        if (tray.available) {
            close.accepted = false
            window.hide()
        }
    }
}
