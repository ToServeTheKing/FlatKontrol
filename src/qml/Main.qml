// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import org.kde.kirigamiaddons.delegates as Delegates
import io.github.toservetheking.FlatKontrol

Kirigami.ApplicationWindow {
    id: root

    title: i18nc("@title:window", "FlatKontrol")

    minimumWidth: Kirigami.Units.gridUnit * 32
    minimumHeight: Kirigami.Units.gridUnit * 24
    width: Kirigami.Units.gridUnit * 48
    height: Kirigami.Units.gridUnit * 34

    ApplicationsModel {
        id: appsModel
    }

    PermissionsController {
        id: controller
    }

    KItemModels.KSortFilterProxyModel {
        id: filteredApps
        sourceModel: appsModel
        filterRoleName: "name"
        filterCaseSensitivity: Qt.CaseInsensitive
        sortRoleName: "name"
    }

    pageStack.defaultColumnWidth: Kirigami.Units.gridUnit * 16
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.ToolBar

    // Two static columns (sidebar + permissions) - no dynamic page pushing.
    pageStack.initialPage: [sidebarComponent, permsComponent]

    function showDetails(): void {
        detailsDialog.open();
    }

    function selectApp(appId: string): void {
        controller.appId = appId;
        // On narrow layouts, reveal the permissions column.
        root.pageStack.currentIndex = 1;
    }

    Component {
        id: sidebarComponent

        Kirigami.ScrollablePage {
            title: i18nc("@title", "Applications")

            titleDelegate: Kirigami.SearchField {
                Layout.fillWidth: true
                onTextChanged: filteredApps.filterString = text
            }

            ListView {
                id: appList
                model: filteredApps
                currentIndex: -1

                delegate: Delegates.RoundedItemDelegate {
                    id: appDelegate

                    required property int index
                    required property string appId
                    required property string name
                    required property string iconSource
                    required property bool isGlobal

                    text: name
                    icon.source: iconSource
                    highlighted: controller.appId === appId

                    contentItem: Delegates.SubtitleContentItem {
                        itemDelegate: appDelegate
                        subtitle: appDelegate.isGlobal ? i18n("Default settings for all apps") : appDelegate.appId
                    }

                    onClicked: {
                        appList.currentIndex = index;
                        root.selectApp(appId);
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 4
                    visible: appList.count === 0
                    icon.name: "flatpak-symbolic"
                    text: i18n("No Flatpak applications found")
                }
            }
        }
    }

    Component {
        id: permsComponent
        PermissionsPage {
            controller: controller
        }
    }

    Kirigami.Dialog {
        id: detailsDialog
        title: i18nc("@title", "Application Details")
        standardButtons: QQC2.Dialog.Close
        preferredWidth: Kirigami.Units.gridUnit * 24
        padding: Kirigami.Units.largeSpacing

        Kirigami.FormLayout {
            QQC2.Label {
                Kirigami.FormData.label: i18n("Name:")
                text: controller.appName
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Application ID:")
                text: controller.appId
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Version:")
                text: controller.appVersion
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Runtime:")
                text: controller.appRuntime
            }
        }
    }
}
