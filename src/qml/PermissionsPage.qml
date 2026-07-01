// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import Qt.labs.qmlmodels
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import org.kde.kirigamiaddons.formcard as FormCard
import io.github.toservetheking.FlatKontrol

Kirigami.ScrollablePage {
    id: page

    required property PermissionsController controller

    readonly property bool hasSelection: controller.appId.length > 0

    title: hasSelection ? controller.appName : i18nc("@title", "Permissions")

    leftPadding: 0
    rightPadding: 0
    topPadding: Kirigami.Units.gridUnit
    bottomPadding: Kirigami.Units.gridUnit

    // A small status marker used across delegates.
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
        spacing: Kirigami.Units.largeSpacing
        visible: page.hasSelection

        Repeater {
            model: page.controller.categories

            delegate: ColumnLayout {
                id: section

                required property var modelData

                Layout.fillWidth: true
                spacing: 0

                FormCard.FormHeader {
                    title: section.modelData.title
                }

                FormCard.FormCard {
                    KItemModels.KSortFilterProxyModel {
                        id: catModel
                        sourceModel: page.controller
                        filterRoleName: "categoryId"
                        // No category id is a substring of another, so this is an exact match.
                        filterString: section.modelData.id
                    }

                    Repeater {
                        model: catModel

                        delegate: DelegateChooser {
                            role: "rowType"

                            // Toggle
                            DelegateChoice {
                                roleValue: 0
                                delegate: FormCard.FormSwitchDelegate {
                                    id: toggleD
                                    required property int index
                                    required property string label
                                    required property string example
                                    required property bool value
                                    required property int status
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    text: label
                                    description: example
                                    checked: value
                                    icon.name: status === 2 ? "document-edit-symbolic" : (status === 1 ? "globe-symbolic" : "")
                                    onToggled: page.controller.setToggleValue(sourceRow, checked)
                                }
                            }

                            // Filesystem path (with access mode + Browse)
                            DelegateChoice {
                                roleValue: 1
                                delegate: FormCard.AbstractFormDelegate {
                                    id: fsD
                                    required property int index
                                    required property string value
                                    required property string secondary
                                    required property int status
                                    required property bool removable
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    background: null

                                    FolderDialog {
                                        id: folderDialog
                                        onAccepted: {
                                            const p = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""));
                                            fsField.text = p;
                                            page.controller.setEntryPrimary(fsD.sourceRow, p);
                                        }
                                    }

                                    contentItem: RowLayout {
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
                                }
                            }

                            // Persistent (home-relative) path
                            DelegateChoice {
                                roleValue: 2
                                delegate: FormCard.AbstractFormDelegate {
                                    id: relD
                                    required property int index
                                    required property string value
                                    required property int status
                                    required property bool removable
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    background: null
                                    contentItem: RowLayout {
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
                                }
                            }

                            // Environment variable
                            DelegateChoice {
                                roleValue: 3
                                delegate: FormCard.AbstractFormDelegate {
                                    id: varD
                                    required property int index
                                    required property string value
                                    required property string secondary
                                    required property int status
                                    required property bool removable
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    background: null
                                    contentItem: RowLayout {
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
                                }
                            }

                            // D-Bus name policy
                            DelegateChoice {
                                roleValue: 4
                                delegate: FormCard.AbstractFormDelegate {
                                    id: busD
                                    required property int index
                                    required property string value
                                    required property string secondary
                                    required property int status
                                    required property bool removable
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    background: null
                                    contentItem: RowLayout {
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
                                }
                            }

                            // Portal (tri-state)
                            DelegateChoice {
                                roleValue: 5
                                delegate: FormCard.FormComboBoxDelegate {
                                    id: portalD
                                    required property int index
                                    required property string label
                                    required property string example
                                    required property int value
                                    required property bool supported
                                    required property string unsupportedReason
                                    readonly property int sourceRow: catModel.mapToSource(catModel.index(index, 0)).row
                                    readonly property var states: [2, 4, 3]
                                    text: label
                                    description: supported ? example : unsupportedReason
                                    enabled: supported
                                    model: [i18n("Unset"), i18n("Allowed"), i18n("Disallowed")]
                                    currentIndex: value === 4 ? 1 : (value === 3 ? 2 : 0)
                                    onActivated: page.controller.setPortalValue(sourceRow, states[currentIndex])
                                }
                            }

                            // "Add entry" affordance
                            DelegateChoice {
                                roleValue: 6
                                delegate: FormCard.FormButtonDelegate {
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
