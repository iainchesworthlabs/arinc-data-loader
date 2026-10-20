# Languages and colour

## Language

Only the **loader** can change language. **Preferences > Language** lists eight: English,
French (Français), Spanish (Español), German (Deutsch), Yiddish (ייִדיש), Hebrew (עברית), Simplified
Chinese (中文) and Japanese (日本語). The choice applies to the open window at once and is
remembered. On the first start the loader uses the system's language if it is one of these,
otherwise English.

**What is and is not translated.** The translations are a first, machine-made draft that no native
speaker has reviewed. Only 148 of the 515 pieces of text in the window have been translated; the
rest still show in English, which is why a translated window is a mixture. Some text is never
translated because it does not belong to the window: the names of notices and outcomes in the log
("status file repeated", "data load protocol time-out"), the standard's findings about a media
set, and whatever the target itself says. Hebrew and Yiddish are written right to left; the window sets the right-to-left direction (text such
as the log lines is right-aligned), but **the layout is not mirrored yet**: the rail stays on the left
and the buttons keep their places.

![The loader in Hebrew](../images/app-workbench-hebrew.png)

The **generator** and the **mock target** are in English only and have no language setting.

## Colour

The three programs follow the desktop's colour scheme: light when the desktop is light, dark when
it is dark. There is no switch inside the programs. To change it, change the setting of the
system (on Windows, *Settings > Personalisation > Colours > Choose your mode*). The programs follow it while they run; if one does not, restart it.

![The loader in the dark scheme](../images/app-upload-complete-dark.png)

The colours are a deliberately small set: neutral greys and one red. Success is shown in the neutral
colour and a word ("COMPLETE"), a warning or a failure in red and a word, so the meaning never
depends on telling two colours apart.

The programs have no text size setting of their own yet.
