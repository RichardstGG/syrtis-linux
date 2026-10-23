import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// QuotaView.swift's limits lens for the three Linux quota providers.
ScrollView {
    id: root
    required property QtObject service
    objectName: "quotaView"
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Agent limits")
                font.pixelSize: 24
                font.bold: true
                Layout.fillWidth: true
            }
            Button {
                objectName: "quotaRefresh"
                text: root.service && root.service.busy ? qsTr("Checking…") : qsTr("Refresh")
                enabled: root.service !== null && !root.service.busy
                onClicked: root.service.refresh()
            }
        }
        QuotaSummaryLine {
            summary: root.service ? root.service.summary : ({})
            Layout.fillWidth: true
        }
        Repeater {
            model: root.service ? root.service.cards : []
            delegate: AgentLimitsCard {
                required property var modelData
                card: modelData
                Layout.fillWidth: true
            }
        }
    }
}
