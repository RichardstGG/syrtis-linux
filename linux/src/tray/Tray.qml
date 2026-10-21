import QtQuick
import Qt.labs.platform as Platform

Platform.SystemTrayIcon {
    id: tray

    required property Window window

    visible: available
    tooltip: qsTr("Syrtis")
    icon.source: Qt.resolvedUrl("tray-icon.svg")

    function showWindow() {
        window.showNormal()
        window.raise()
        // The compositor decides whether activation is permitted on Wayland.
        window.requestActivate()
    }

    function toggleWindow() {
        if (window.visible && window.visibility !== Window.Minimized)
            window.hide()
        else
            showWindow()
    }

    onActivated: function(reason) {
        if (reason === Platform.SystemTrayIcon.Trigger
                || reason === Platform.SystemTrayIcon.DoubleClick)
            toggleWindow()
    }

    menu: Platform.Menu {
        Platform.MenuItem {
            objectName: "trayToggleAction"
            text: tray.window.visible && tray.window.visibility !== Window.Minimized
                  ? qsTr("Hide window") : qsTr("Show window")
            onTriggered: tray.toggleWindow()
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            objectName: "trayQuitAction"
            text: qsTr("Quit")
            onTriggered: Qt.quit()
        }
    }
}
