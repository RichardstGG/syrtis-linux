import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Port of QuotaSummaryLine.swift's tightest row; QuotaSummaryFold owns choice.
Frame {
    id: root
    required property var summary
    objectName: "quotaSummary"
    visible: summary.name !== undefined

    ColumnLayout {
        width: parent.width
        Label { text: qsTr("Tightest"); font.bold: true }
        Label {
            objectName: "quotaSummaryName"
            text: root.summary.name === undefined ? "" : root.summary.name + " · " + root.summary.label
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label { text: root.summary.remainingText || "" }
        Label { text: root.summary.resetText || ""; visible: text.length > 0 }
    }
}
