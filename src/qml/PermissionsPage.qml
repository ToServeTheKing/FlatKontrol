// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import Qt.labs.qmlmodels
import org.kde.kirigami as Kirigami
import io.github.toservetheking.FlatKontrol

Kirigami.ScrollablePage {
    id: page

    required property PermissionsController controller

    readonly property bool hasSelection: controller.appId.length > 0

    title: hasSelection ? controller.appName : i18nc("@title", "Permissions")

    // --- shared bits ---------------------------------------------------------

    component StatusBadge: Kirigami.Icon {
        property int badgeStatus: 0
        visible: badgeStatus > 0
        implicitWidth: Kirigami.Units.iconSizes.small
        implicitHeight: Kirigami.Units.iconSizes.small
        source: badgeStatus === 2 ? "document-edit" : "globe"

        HoverHandler {
            id: hover
        }
        QQC2.ToolTip.visible: hover.hovered
        QQC2.ToolTip.text: badgeStatus === 2 ? i18n("Set by you") : i18n("Set by a global override")
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

    // --- the list ------------------------------------------------------------

    ListView {
        id: permsView
        model: page.controller

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: !page.hasSelection
            icon.name: "preferences-desktop"
            text: i18n("Select an application")
            explanation: i18n("Choose an application to review and adjust its permissions.")
        }

        section.property: "categoryTitle"
        section.delegate: Kirigami.ListSectionHeader {
            required property string section
            width: ListView.view.width
            text: section
        }

        delegate: DelegateChooser {
            role: "rowType"

            // Toggle (share/sockets/devices/features/filesystem presets)
            DelegateChoice {
                roleValue: 0
                delegate: QQC2.ItemDelegate {
                    id: toggleDelegate
                    required property int index
                    required property string label
                    required property string example
                    required property bool value
                    required property int status

                    width: ListView.view.width
                    hoverEnabled: true
                    onClicked: toggleSwitch.toggle()

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            QQC2.Label {
                                text: toggleDelegate.label
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            QQC2.Label {
                                text: toggleDelegate.example
                                visible: text.length > 0
                                opacity: 0.6
                                font: Kirigami.Theme.smallFont
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                        StatusBadge {
                            badgeStatus: toggleDelegate.status
                        }
                        QQC2.Switch {
                            id: toggleSwitch
                            checked: toggleDelegate.value
                            onToggled: page.controller.setToggleValue(toggleDelegate.index, checked)
                        }
                    }
                }
            }

            // Filesystem path entry
            DelegateChoice {
                roleValue: 1
                delegate: page.entryDelegate
            }
            // Persistent (relative path) entry
            DelegateChoice {
                roleValue: 2
                delegate: page.entryDelegate
            }

            // Environment variable
            DelegateChoice {
                roleValue: 3
                delegate: QQC2.ItemDelegate {
                    id: varDelegate
                    required property int index
                    required property string value
                    required property string secondary
                    required property int status
                    required property bool removable
                    width: ListView.view.width
                    hoverEnabled: true
                    background: null

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing
                        QQC2.TextField {
                            text: varDelegate.value
                            placeholderText: i18n("VARIABLE")
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                            onTextEdited: page.controller.setEntryPrimary(varDelegate.index, text)
                        }
                        QQC2.Label {
                            text: "="
                        }
                        QQC2.TextField {
                            text: varDelegate.secondary
                            placeholderText: i18n("value")
                            Layout.fillWidth: true
                            onTextEdited: page.controller.setEntrySecondary(varDelegate.index, text)
                        }
                        StatusBadge {
                            badgeStatus: varDelegate.status
                        }
                        QQC2.ToolButton {
                            icon.name: "edit-delete-remove"
                            visible: varDelegate.removable
                            onClicked: page.controller.removeEntry(varDelegate.index)
                        }
                    }
                }
            }

            // D-Bus name policy
            DelegateChoice {
                roleValue: 4
                delegate: QQC2.ItemDelegate {
                    id: busDelegate
                    required property int index
                    required property string value
                    required property string secondary
                    required property int status
                    required property bool removable
                    width: ListView.view.width
                    hoverEnabled: true
                    background: null

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing
                        QQC2.TextField {
                            text: busDelegate.value
                            placeholderText: i18n("e.g. org.freedesktop.Notifications")
                            Layout.fillWidth: true
                            onTextEdited: page.controller.setEntryPrimary(busDelegate.index, text)
                        }
                        QQC2.ComboBox {
                            model: ["talk", "own"]
                            currentIndex: busDelegate.secondary === "own" ? 1 : 0
                            onActivated: page.controller.setEntrySecondary(busDelegate.index, currentValue)
                        }
                        StatusBadge {
                            badgeStatus: busDelegate.status
                        }
                        QQC2.ToolButton {
                            icon.name: "edit-delete-remove"
                            visible: busDelegate.removable
                            onClicked: page.controller.removeEntry(busDelegate.index)
                        }
                    }
                }
            }

            // Portal (tri-state)
            DelegateChoice {
                roleValue: 5
                delegate: QQC2.ItemDelegate {
                    id: portalDelegate
                    required property int index
                    required property string label
                    required property string example
                    required property int value
                    required property bool supported
                    required property string unsupportedReason
                    width: ListView.view.width
                    hoverEnabled: true
                    background: null

                    readonly property var states: [2, 4, 3] // Unset, Allowed, Disallowed

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            QQC2.Label {
                                text: portalDelegate.label
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            QQC2.Label {
                                text: portalDelegate.supported ? portalDelegate.example : portalDelegate.unsupportedReason
                                visible: text.length > 0
                                opacity: 0.6
                                font: Kirigami.Theme.smallFont
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }
                        QQC2.ComboBox {
                            enabled: portalDelegate.supported
                            model: [i18n("Unset"), i18n("Allowed"), i18n("Disallowed")]
                            currentIndex: portalDelegate.value === 4 ? 1 : (portalDelegate.value === 3 ? 2 : 0)
                            onActivated: page.controller.setPortalValue(portalDelegate.index, portalDelegate.states[currentIndex])
                        }
                    }
                }
            }

            // "Add entry" affordance
            DelegateChoice {
                roleValue: 6
                delegate: QQC2.ItemDelegate {
                    id: addDelegate
                    required property string categoryId
                    width: ListView.view.width
                    icon.name: "list-add"
                    text: i18nc("@action", "Add…")
                    onClicked: page.controller.addEntry(categoryId)
                }
            }
        }
    }

    // Shared delegate for path / relative-path entries.
    property Component entryDelegate: Component {
        QQC2.ItemDelegate {
            id: pathDelegate
            required property int index
            required property string value
            required property string categoryId
            required property int status
            required property bool removable
            width: ListView.view.width
            hoverEnabled: true
            background: null

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing
                QQC2.TextField {
                    text: pathDelegate.value
                    placeholderText: pathDelegate.categoryId === "persistent" ? i18n("e.g. .thunderbird") : i18n("e.g. ~/games:ro")
                    Layout.fillWidth: true
                    onTextEdited: page.controller.setEntryPrimary(pathDelegate.index, text)
                }
                StatusBadge {
                    badgeStatus: pathDelegate.status
                }
                QQC2.ToolButton {
                    icon.name: "edit-delete-remove"
                    visible: pathDelegate.removable
                    onClicked: page.controller.removeEntry(pathDelegate.index)
                }
            }
        }
    }
}
