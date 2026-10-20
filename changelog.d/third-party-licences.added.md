**Third-party licences in the applications.** About has a Licences button in all
three applications that opens the notices this build carries: Qt under the LGPL
(with the text of the licence and the offer of its source), the Archivo typeface
under the SIL Open Font License, the runtime files Qt's deployment tool adds on
Windows, and Tracy in a profiling build. `cmake/Notices.cmake` assembles
`NOTICES.txt` at configure time from `notices/fragments` and the licence texts
in `notices/licences`, embeds it in each executable and places it, with
`LICENSE.txt`, beside them; `system.notices` checks the result.
[docs/licensing.md](docs/licensing.md) says how to add a component.
