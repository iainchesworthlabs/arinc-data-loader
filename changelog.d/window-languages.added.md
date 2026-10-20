**The window in eight languages** ([docs/app.md](docs/app.md)): catalogues for French, Spanish,
German, Yiddish, Hebrew, Simplified Chinese and Japanese beside the English source, built by
`qt_add_translations` into the window's resources and installed by `LanguageManager`; the
right-to-left scripts set the application's layout direction (the layout of the window is not
mirrored: no `LayoutMirroring` is set in the QML, a known defect) and the language preference is
persisted. The text is a first, mechanical draft, to be reviewed by speakers; the window test
installs each catalogue in turn and checks the retranslation and the layout direction.
