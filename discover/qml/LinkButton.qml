import QtQuick 2.6
import QtGraphicalEffects 1.0
import QtQuick.Templates 2.0 as T
import org.kde.kirigami 1.0

T.ToolButton {
    id: control

    property alias shadow: shadow.visible
    property Action action: null
    text: action ? action.text : ""
    enabled: !action || action.enabled
    onClicked: if (action) action.trigger()

    implicitWidth: textItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: textItem.implicitHeight + topPadding + bottomPadding
    baselineOffset: contentItem.y + contentItem.baselineOffset

    readonly property alias textColor: textItem.color
    hoverEnabled: true

    onHoveredChanged: {
        textItem.font.underline = hovered && enabled
    }

    contentItem: Text {
        id: textItem
        text: control.text
        font: control.font
        color: control.shadow ? Theme.viewBackgroundColor : Theme.linkColor
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    DropShadow {
        id: shadow
        horizontalOffset: 2
        verticalOffset: 2
        radius: 8.0
        samples: 17
        color: "#f0000000"
        source: textItem
        anchors.fill: textItem
    }

    background: null
}