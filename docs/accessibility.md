---
template: marketing.html
title: Accessibility
title_tag: "Accessibility: ARINC Data Loader"
description: What the ARINC Data Loader programs and this website do for accessibility, and what has not been checked.
layout: prose
---
# Accessibility

This page says what the programs do for accessibility, and what has not been checked. It is not a statement of conformance with any standard. No screen reader has been tried with the programs, and none of the checks below was made on a platform other than the one named.

## What the programs do

**Keyboard.** The data loader has had a keyboard pass with automated tests. A focused text field, number field, FIND answer or tab shows a ring two pixels wide. A dialog puts the focus inside it when it opens and gives it back when it closes, and Escape closes it. The FIND answers and the tabs are each one tab stop and move with the arrow keys. The activity log scrolls with the keyboard and copies with the system's Copy key. Ctrl+F runs FIND. The media set generator and the mock target use the same controls but have not had a keyboard pass, and have no automated keyboard test. The [keyboard page](user-guide/keyboard.md) lists the keys.

**Names for assistive technology.** Buttons, fields, drop-downs and number fields have names in the code, and each button's name is its text. For the loader, tests check the names of nine buttons and of the identifier fields, and that a field set from outside reaches the program. The generator and the mock target have no test of names, and in a few places their name is longer than the text on the screen (the About button is "About the generator", for example). Some lists have a name for the list and none for each row: the activity log, the file lists of the generator and the mock target, and the two lists in the media set properties.

**Colour.** The programs follow the system's light or dark setting. A status is shown by a symbol and a word as well as a colour, and a test checks that each kind of status has its own symbol. A test checks that the text of each status has at least 4.5 to 1 contrast against the surface it sits on, and that borders and progress bars have at least 3 to 1, in both schemes. The design used the WCAG 2.2 AA contrast ratios as a guide. The programs have not been checked against the standard as a whole.

**Motion.** Two things animate: the busy mark and the sweep of a progress bar that has no known length. When the system asks for reduced motion, both stand still. The code does this, and no test sets the system preference.

**Languages.** The programs come in eight languages. Hebrew and Yiddish mirror the whole window, which tests check for the loader and, for the language picker only, for the other two. Seven of the translations are machine drafts that no speaker has read.

**Text size.** The type follows the display scaling of the system. The programs have no text size setting of their own, and the system's text size setting is not used. Nothing has been tried at a display scale other than 100 percent.

## What has not been checked, or does not exist

- No screen reader has been tried: not NVDA, JAWS, Narrator, VoiceOver or Orca. The tests read the names in the program, not what the system's accessibility service passes to a screen reader.
- The programs do not announce changes, and whether a screen reader speaks a banner when it appears is not known.
- The programs have no high-contrast mode. Their colours are fixed.
- The programs do not use the system's text size setting.
- Windows is the only system on which the programs have been driven by hand. Linux and macOS have been built and tested in continuous integration, and nobody has driven the windows.
- The keyboard pass has not been made on the generator or the mock target.
- The translations have not been reviewed.

## This website

The front pages are plain HTML. They use no script, follow the system's light or dark setting, and underline their links. The guide and the pages for programmers are built with Material for MkDocs, and their search needs a script. The colours of the site's palette were checked against the company's rules for contrast, in both schemes. The pages have not been run through an accessibility testing tool, and have not been tried with a screen reader.

## Report a barrier

Use [Support](support.md). Say which program and which system, and what you were trying to do.
