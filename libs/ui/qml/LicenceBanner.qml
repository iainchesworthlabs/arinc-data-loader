// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The licence notice under a window's header: a licence that ends soon, one in its grace period, one that
// has ended, a date that looks wrong. It is never a dialog and never blocks anything. One at a time; the
// words, the kind and the label of the button that puts it away are the licence model's
// (LicenceModel::noticeText, noticeKind, noticeDismissText), and it shows nothing when there is no notice.
AppBanner {
    id: root

    // The LicenceModel, a context property of every program.
    property var model: null

    signal licenceRequested()

    visible: root.model !== null && root.model.noticeKind.length > 0
    tone: root.model !== null && root.model.noticeKind === "warn" ? "warning" : "info"
    text: root.model !== null ? root.model.noticeText : ""

    AppButton {
        objectName: "licenceNoticeOpen"
        text: qsTr("Licence")
        onClicked: root.licenceRequested()
        Accessible.description: qsTr("Open the licence window")
    }

    AppButton {
        objectName: "licenceNoticeDismiss"
        text: root.model !== null ? root.model.noticeDismissText : ""
        onClicked: root.model.dismissNotice()
    }
}
