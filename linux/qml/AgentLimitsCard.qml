import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Port of AgentLimitsCard.swift's remaining-window cards. Presentation values
// arrive from QuotaService; QML never formats or reconstructs quota data.
Frame {
    id: root
    required property var card
    objectName: "quotaCard-" + card.id
    padding: 16
    Accessible.role: Accessible.Grouping
    Accessible.name: card.name + " " + card.accountLabel + ": " + card.state

    ColumnLayout {
        width: parent.width
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: root.card.name
                font.bold: true
                font.pixelSize: 18
            }
            Label {
                text: root.card.accountLabel
                visible: text.length > 0
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            Item { Layout.fillWidth: true }
            Label {
                objectName: "quotaStatus"
                text: root.card.state
                font.bold: true
                color: root.card.state === "error" ? "#e05b5b"
                    : root.card.state === "stale" ? "#c18419" : palette.text
            }
        }

        Label {
            objectName: "quotaMessage"
            text: root.card.message
            visible: text.length > 0
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Repeater {
            model: root.card.windows
            delegate: ColumnLayout {
                required property var modelData
                objectName: "quotaWindow-" + modelData.id
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: modelData.label
                        font.bold: true
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    Label {
                        objectName: "quotaRemaining"
                        text: modelData.remainingText
                    }
                }
                ProgressBar {
                    objectName: "quotaGauge"
                    from: 0
                    to: 100
                    value: modelData.remainingPercent
                    Layout.fillWidth: true
                    Accessible.name: modelData.label + ": " + modelData.remainingText
                }
                Label {
                    objectName: "quotaReset"
                    text: modelData.resetText
                    visible: text.length > 0
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}
