// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The shape that says what a status word says, so a state is never colour alone: a check, a warning
// triangle with a bar, a cross, a play mark (work in progress) or a dot. Theme.toneGlyph(tone)
// names the shape for a tone. Drawn as paths rather than typed as characters because no bundled
// face carries them and the fallback face differs on every desktop.
Canvas {
    id: root

    // check | warning | cross | busy | dot
    property string glyph: "dot"
    property color color: Theme.text

    implicitWidth: 12
    implicitHeight: 12
    Accessible.ignored: true

    onGlyphChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const c = getContext("2d");
        c.reset();
        const w = width;
        const h = height;
        c.strokeStyle = root.color;
        c.fillStyle = root.color;
        c.lineWidth = Math.max(1.5, w / 7);
        c.lineJoin = "miter";
        c.lineCap = "butt";
        c.beginPath();
        switch (root.glyph) {
        case "check":
            c.moveTo(w * 0.08, h * 0.55);
            c.lineTo(w * 0.38, h * 0.85);
            c.lineTo(w * 0.94, h * 0.18);
            c.stroke();
            break;
        case "cross":
            c.moveTo(w * 0.14, h * 0.14);
            c.lineTo(w * 0.86, h * 0.86);
            c.moveTo(w * 0.86, h * 0.14);
            c.lineTo(w * 0.14, h * 0.86);
            c.stroke();
            break;
        case "warning":
            c.lineWidth = Math.max(1.2, w / 9);
            c.moveTo(w * 0.5, h * 0.06);
            c.lineTo(w * 0.96, h * 0.92);
            c.lineTo(w * 0.04, h * 0.92);
            c.closePath();
            c.stroke();
            c.beginPath();
            c.moveTo(w * 0.5, h * 0.36);
            c.lineTo(w * 0.5, h * 0.62);
            c.stroke();
            c.fillRect(w * 0.5 - c.lineWidth / 2, h * 0.72, c.lineWidth, c.lineWidth);
            break;
        case "busy":
            c.moveTo(w * 0.22, h * 0.08);
            c.lineTo(w * 0.92, h * 0.5);
            c.lineTo(w * 0.22, h * 0.92);
            c.closePath();
            c.fill();
            break;
        default:
            c.arc(w / 2, h / 2, Math.min(w, h) * 0.22, 0, Math.PI * 2);
            c.fill();
            break;
        }
    }
}
