#pragma once

namespace adl::ui {

// Registers the bundled Archivo faces and makes Archivo the application's default typeface.
// Call after the QGuiApplication is constructed and before the QML engine loads: Theme.qml
// probes Qt.fontFamilies() for the family, and every standard control follows the application
// font.
void register_fonts();

}  // namespace adl::ui
