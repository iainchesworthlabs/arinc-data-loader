# Find a target and read its information

## Find a target (FIND)

FIND asks the whole local network "who is a target?" and lists every target that answers. Press
**Find** in the **01 Target** block (or **Ctrl+F**). The loader says "Looking for targets
(broadcast, three seconds)...", waits three seconds, and then reports how many answered.

![The targets that answered a FIND](../images/app-find-results.png)

Each answer is a row under **FOUND**: the target's `THW_ID_POS` in bold, and under it its address,
type name and position. **Click a row** to choose it: its address and `THW_ID_POS` are copied into
the two fields and the row is marked **SELECTED**. **Properties...** then shows everything the
target said: hardware ID, type name, position, literal name and manufacturer code.

If the FIND answer gives a hardware ID and a position, the loader makes `THW_ID_POS` from
them with an underscore (`HNPFMS` at `L` becomes `HNPFMS_L`), and from the identifier alone when
there is no position. That reading follows the examples in the standard and has not been checked
against a real target. The field can always be edited.

**If nothing answers.** FIND goes out as a broadcast, which does not cross routers and which a
firewall may block. Targets that do not answer FIND at all, or that you reach over AFDX (a network
that has no broadcast), are named by hand: type the address and the `THW_ID_POS` into the fields.
That always works if the target is reachable. See
[Troubleshooting](troubleshooting.md#find-finds-nothing).

**FIND and an operation together.** In [Preferences](preferences.md), **Find the target
automatically** makes the loader run FIND by itself when you start an operation with no target
chosen. One answer is taken; none, or several, leaves the choice to you.

## Name a target by hand

Type the IPv4 address in **Address** and the identifier in **THW_ID_POS**. The chip says **SET**
when the address is four numbers from 0 to 255 and the identifier is not empty. A wrong address
gives, when you start an operation, the message ""abc" is not an IPv4 address." with the hint to
use four numbers of 0 to 255 separated by dots, for example `10.0.0.2`. The last target an
operation was started against is remembered and put back next time.

## Read information

**Read information** (in the action bar, in the Upload view, and in the dock) runs the *Information*
operation: the loader asks the target to describe itself. It does not change anything on the
target. The log shows "Asking 127.0.0.1 (HNPFMS_L) for its configuration...", then what the
target answers and each status file it writes. When it ends, the **TARGET CONFIGURATION** tab of
the dock lists the target's hardware (with serial numbers) and the software parts installed in it.

![The target's configuration](../images/app-information.png)

An Information operation can be stopped with **Abort** like the others.

Next: [choose and check a media set](choose-a-media-set.md).
