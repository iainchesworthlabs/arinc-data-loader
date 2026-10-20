# Keyboard

The windows can be used without a mouse. This page says what works in the **loader**, which has had
a keyboard pass and automated tests for it; the generator and the mock target follow the same
rules but have not had that pass yet. None of it has been checked with a screen reader (the
controls carry accessible names that are the text on the screen, so one should announce them, but
that is untested).

## Moving around

- **Tab** moves to the next control and **Shift+Tab** to the previous one. Every control you can
  press is a tab stop; a dimmed (disabled) control is skipped. The control that has the focus is
  outlined with a 2 pixel ring, in a text field, a drop-down list or a number field as well.
- **Space** or **Return** presses a button and ticks or unticks a check box.
- The **Upload / Download** switch, the **ACTIVITY / TARGET CONFIGURATION** tabs, the
  **Media defined / Operator defined** choice and the **targets FIND lists** are a single tab stop
  each: use the **arrow keys** (and **Home** and **End**) to choose between their parts, and the
  focus moves with the choice. Choosing is immediate; pressing the part that is already chosen
  leaves it chosen. In the list of targets, **Return** or **Space** chooses the one you are on.
- The **activity log**, the **target configuration** and the lists in the media set properties are
  stops of their own: **Up**, **Down**, **Page Up**, **Page Down**, **Home** and **End** scroll them.
  **Ctrl+C** in the activity log copies every line, as "time  SEVERITY  message".
- A page that is longer than the window scrolls to follow the control you tab to.
- In a drop-down list (a check value, the language, the refusal choice of the mock target) the
  arrow keys move through the entries, and **Return** or **Space** opens and confirms.
- In a number field (a port, a time, a count) type the number, or use the **up** and **down**
  arrow keys, or the **−** and **+** buttons.

## Dialogs

A dialog (Preferences, the properties pages, About, Licences, the question and error dialogs)
keeps the keyboard to itself while it is open, and opens with the focus inside it: on the first
field in Preferences, on the main button in the others. **Esc** closes it without applying
anything (on the error dialog it acknowledges the failure, as **OK** does). **Return** or **Enter**
in a dialog presses its main button (Save, Close or OK; the filled one) unless you are on another
button or a control that uses the key. The one exception is the question that appears when you
close the loader during a transfer: it starts on **Keep working** and **Return** does not stop the
transfer, so stopping takes a deliberate Tab to **Stop and close**. When a dialog closes, the focus
goes back to the control you opened it from.

The system's own folder and file choosers (the media set, the download folder, the generator's
files and output folder) are the operating system's, with its usual keys.

## Shortcuts

| Shortcut | Window | What it does |
| --- | --- | --- |
| **Ctrl+F** | Loader | Runs FIND (when nothing else is running) |
| The system's *Preferences* key (on a Mac, **Cmd+,**) | Loader | Opens Preferences |
| The system's *Quit* key (**Ctrl+Q** on Linux, **Cmd+Q** on a Mac) | All three | Closes the window |

On Windows the system defines no Quit key; close a window with its close button or **Alt+F4**. If
the loader is busy, closing asks first.

## Voice control and automation

A text or number field can also be set from outside (by a screen reader's or voice control's
"set value" action, or by UI Automation); the loader takes the new text as if it had been typed.
