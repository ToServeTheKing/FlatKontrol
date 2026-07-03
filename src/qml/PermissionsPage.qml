/*
    SPDX-FileCopyrightText: 2026 ToServeTheKing <austin@thebennett.net>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

pragma ComponentBehavior: Bound

// Styled after KDE System Settings' Audio page (plasma-pa's kcm/ui/main.qml):
// Kirigami.ListSectionHeader per category, flat rows indented under the
// header and separated by Kirigami.Separator, no card/border around them.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import Qt.labs.qmlmodels
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import io.github.toservetheking.FlatKontrol

Kirigami.ScrollablePage {
    id: page

    required property PermissionsController controller

    readonly property bool hasSelection: controller.appId.length > 0

    title: hasSelection ? controller.appName : i18nc("@title", "Permissions")

    leftPadding: 0
    rightPadding: 0
    // No topPadding: Kirigami.ListSectionHeader (the first thing on the
    // page) already carries its own top padding, and stacking ours on top
    // of that left an oversized gap above the first category.
    bottomPadding: Kirigami.Units.gridUnit

    // A small status marker used across rows.
    component StatusBadge: Kirigami.Icon {
        property int state: 0
        visible: state > 0
        implicitWidth: Kirigami.Units.iconSizes.small
        implicitHeight: Kirigami.Units.iconSizes.small
        source: state === 2 ? "document-edit-symbolic" : "globe-symbolic"
        HoverHandler {
            id: badgeHover
        }
        QQC2.ToolTip.visible: badgeHover.hovered
        QQC2.ToolTip.text: state === 2 ? i18n("Set by you") : i18n("Set by a global override")
    }

    actions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Save")
            icon.name: "document-save"
            enabled: page.controller.modified
            onTriggered: page.controller.save()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Reset")
            icon.name: "edit-reset"
            enabled: page.hasSelection
            onTriggered: page.controller.reset()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Undo Reset")
            icon.name: "edit-undo"
            visible: page.controller.canUndo
            onTriggered: page.controller.undo()
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Details")
            icon.name: "documentinfo"
            visible: page.hasSelection && !page.controller.isGlobal
            onTriggered: applicationWindow().showDetails()
        }
    ]

    Connections {
        target: page.controller
        function onSaved() {
            applicationWindow().showPassiveNotification(i18n("Permissions saved"));
        }
        function onError(message) {
            applicationWindow().showPassiveNotification(message, "long");
        }
    }

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        visible: !page.hasSelection
        icon.name: "preferences-desktop"
        text: i18n("Select an application")
        explanation: i18n("Choose an application to review and adjust its permissions.")
    }

    ColumnLayout {
        spacing: 0
        visible: page.hasSelection

        Repeater {
            model: page.controller.categories

            delegate: ColumnLayout {
                id: section

                required property var modelData

                Layout.fillWidth: true
                spacing: 0

                Kirigami.ListSectionHeader {
                    Layout.fillWidth: true
                    text: section.modelData.title
                }

                KItemModels.KSortFilterProxyModel {
                    id: catModel
                    sourceModel: page.controller
                    filterRoleName: "categoryId"
                    // No category id is a substring of another, so this is an exact match.
                    filterString: section.modelData.id
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.largeSpacing

                    Repeater {
                        id: rowRepeater
                        model: catModel

                        delegate: DelegateChooser {
                            role: "rowType"

                            // Toggle
                            DelegateChoice {
                                roleValue: 0
                                delegate: ColumnLayout {
                                    id: toggleD
                                    required property int index
                                    required property string label
                                    required property string example
                                    required property bool value
                                    required property int status
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 0
                                            QQC2.Label {
                                                Layout.fillWidth: true
                                                text: toggleD.label
                                                wrapMode: Text.Wrap
                                            }
                                            QQC2.Label {
                                                Layout.fillWidth: true
                                                text: toggleD.example
                                                visible: text.length > 0
                                                wrapMode: Text.Wrap
                                                font: Kirigami.Theme.smallFont
                                                color: Kirigami.Theme.disabledTextColor
                                            }
                                        }
                                        StatusBadge {
                                            state: toggleD.status
                                        }
                                        QQC2.Switch {
                                            checked: toggleD.value
                                            onToggled: page.controller.setToggleValue(toggleD.sourceRow, checked)
                                        }
                                    }

                                    Kirigami.Separator {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Kirigami.Units.smallSpacing
                                        visible: toggleD.index !== rowRepeater.count - 1
                                    }
                                }
                            }

                            // Filesystem path (with access mode + Browse)
                            DelegateChoice {
                                roleValue: 1
                                delegate: ColumnLayout {
                                    id: fsD
                                    required property int index
                                    required property string value
                                    required property string secondary
                                    required property int status
                                    required property bool removable
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing

                                    FolderDialog {
                                        id: folderDialog
                                        onAccepted: {
                                            const p = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""));
                                            fsField.text = p;
                                            page.controller.setEntryPrimary(fsD.sourceRow, p);
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing
                                        QQC2.TextField {
                                            id: fsField
                                            text: fsD.value
                                            placeholderText: i18n("e.g. ~/Downloads")
                                            Layout.fillWidth: true
                                            onTextEdited: page.controller.setEntryPrimary(fsD.sourceRow, text)
                                        }
                                        QQC2.ComboBox {
                                            readonly property var modes: ["ro", "rw", "create"]
                                            model: [i18n("Read-only"), i18n("Read/write"), i18n("Create")]
                                            currentIndex: Math.max(0, modes.indexOf(fsD.secondary))
                                            onActivated: page.controller.setEntrySecondary(fsD.sourceRow, modes[currentIndex])
                                        }
                                        QQC2.ToolButton {
                                            icon.name: "document-open-folder"
                                            QQC2.ToolTip.text: i18n("Browse…")
                                            QQC2.ToolTip.visible: hovered
                                            onClicked: folderDialog.open()
                                        }
                                        StatusBadge {
                                            state: fsD.status
                                        }
                                        QQC2.ToolButton {
                                            icon.name: "edit-delete-remove"
                                            visible: fsD.removable
                                            onClicked: page.controller.removeEntry(fsD.sourceRow)
                                        }
                                    }

                                    Kirigami.Separator {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Kirigami.Units.smallSpacing
                                        visible: fsD.index !== rowRepeater.count - 1
                                    }
                                }
                            }

                            // Persistent (home-relative) path
                            DelegateChoice {
                                roleValue: 2
                                delegate: ColumnLayout {
                                    id: relD
                                    required property int index
                                    required property string value
                                    required property int status
                                    required property bool removable
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing
                                        QQC2.TextField {
                                            text: relD.value
                                            placeholderText: i18n("e.g. .thunderbird")
                                            Layout.fillWidth: true
                                            onTextEdited: page.controller.setEntryPrimary(relD.sourceRow, text)
                                        }
                                        StatusBadge {
                                            state: relD.status
                                        }
                                        QQC2.ToolButton {
                                            icon.name: "edit-delete-remove"
                                            visible: relD.removable
                                            onClicked: page.controller.removeEntry(relD.sourceRow)
                                        }
                                    }

                                    Kirigami.Separator {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Kirigami.Units.smallSpacing
                                        visible: relD.index !== rowRepeater.count - 1
                                    }
                                }
                            }

                            // Environment variable
                            DelegateChoice {
                                roleValue: 3
                                delegate: ColumnLayout {
                                    id: varD
                                    required property int index
                                    required property string value
                                    required property string secondary
                                    required property int status
                                    required property bool removable
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing
                                        QQC2.TextField {
                                            text: varD.value
                                            placeholderText: i18n("VARIABLE")
                                            Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                                            onTextEdited: page.controller.setEntryPrimary(varD.sourceRow, text)
                                        }
                                        QQC2.Label {
                                            text: "="
                                        }
                                        QQC2.TextField {
                                            text: varD.secondary
                                            placeholderText: i18n("value")
                                            Layout.fillWidth: true
                                            onTextEdited: page.controller.setEntrySecondary(varD.sourceRow, text)
                                        }
                                        StatusBadge {
                                            state: varD.status
                                        }
                                        QQC2.ToolButton {
                                            icon.name: "edit-delete-remove"
                                            visible: varD.removable
                                            onClicked: page.controller.removeEntry(varD.sourceRow)
                                        }
                                    }

                                    Kirigami.Separator {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Kirigami.Units.smallSpacing
                                        visible: varD.index !== rowRepeater.count - 1
                                    }
                                }
                            }

                            // D-Bus name policy
                            DelegateChoice {
                                roleValue: 4
                                delegate: ColumnLayout {
                                    id: busD
                                    required property int index
                                    required property string value
                                    required property string secondary
                                    required property int status
                                    required property bool removable
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing
                                        QQC2.TextField {
                                            text: busD.value
                                            placeholderText: i18n("e.g. org.freedesktop.Notifications")
                                            Layout.fillWidth: true
                                            onTextEdited: page.controller.setEntryPrimary(busD.sourceRow, text)
                                        }
                                        QQC2.ComboBox {
                                            model: ["talk", "own"]
                                            currentIndex: busD.secondary === "own" ? 1 : 0
                                            onActivated: page.controller.setEntrySecondary(busD.sourceRow, currentValue)
                                        }
                                        StatusBadge {
                                            state: busD.status
                                        }
                                        QQC2.ToolButton {
                                            icon.name: "edit-delete-remove"
                                            visible: busD.removable
                                            onClicked: page.controller.removeEntry(busD.sourceRow)
                                        }
                                    }

                                    Kirigami.Separator {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Kirigami.Units.smallSpacing
                                        visible: busD.index !== rowRepeater.count - 1
                                    }
                                }
                            }

                            // Portal (tri-state)
                            DelegateChoice {
                                roleValue: 5
                                delegate: ColumnLayout {
                                    id: portalD
                                    required property int index
                                    required property string label
                                    required property string example
                                    required property int value
                                    required property bool supported
                                    required property string unsupportedReason
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    readonly property var states: [2, 4, 3]
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 0
                                            QQC2.Label {
                                                Layout.fillWidth: true
                                                text: portalD.label
                                                wrapMode: Text.Wrap
                                                enabled: portalD.supported
                                            }
                                            QQC2.Label {
                                                Layout.fillWidth: true
                                                text: portalD.supported ? portalD.example : portalD.unsupportedReason
                                                visible: text.length > 0
                                                wrapMode: Text.Wrap
                                                font: Kirigami.Theme.smallFont
                                                color: Kirigami.Theme.disabledTextColor
                                            }
                                        }
                                        QQC2.ComboBox {
                                            enabled: portalD.supported
                                            model: [i18n("Unset"), i18n("Allowed"), i18n("Disallowed")]
                                            currentIndex: portalD.value === 4 ? 1 : (portalD.value === 3 ? 2 : 0)
                                            onActivated: page.controller.setPortalValue(portalD.sourceRow, portalD.states[currentIndex])
                                        }
                                    }

                                    Kirigami.Separator {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Kirigami.Units.smallSpacing
                                        visible: portalD.index !== rowRepeater.count - 1
                                    }
                                }
                            }

                            // "Add entry" affordance
                            DelegateChoice {
                                roleValue: 6
                                delegate: QQC2.Button {
                                    id: addD
                                    required property string categoryId
                                    text: i18nc("@action", "Add…")
                                    icon.name: "list-add"
                                    onClicked: page.controller.addEntry(categoryId)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
