import QtQuick
import "../src/tray"

Window {
    id: window

    width: 800
    height: 600
    visible: true
    title: "Syrtis"
    color: systemPalette.window
    property QtObject quotaService: null

    SystemPalette { id: systemPalette }

    QuotaView {
        anchors.fill: parent
        anchors.margins: 20
        service: window.quotaService
    }

    function updateQuotaPolling() {
        if (!quotaService)
            return
        if (visible && visibility !== Window.Minimized)
            quotaService.start()
        else
            quotaService.stop()
    }
    Component.onCompleted: updateQuotaPolling()
    onVisibilityChanged: updateQuotaPolling()

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
