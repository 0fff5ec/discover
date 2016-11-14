import QtQuick 2.1
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1
import org.kde.discover 1.0
import org.kde.discover.app 1.0
import org.kde.kirigami 1.0 as Kirigami

ApplicationsListPage {
    id: page
    stateFilter: 2
    sortRole: "ratingCount"
    sortOrder: Qt.DescendingOrder

    title: i18n("Installed")
    compact: true
    canNavigate: false

    header: PageHeader {
        width: parent.width
        background: "qrc:/icons/installedcrop.jpg"

        Item {
            Layout.fillWidth: true
            height: Kirigami.Units.largeSpacing
        }
    }
}
