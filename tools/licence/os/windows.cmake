# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
# Windows: the C runtime calls wmain with the command line as UTF-16, which windows/entry.cpp converts to UTF-8.
set(ADL_LICENCE_OWNER_ENTRY "${CMAKE_CURRENT_LIST_DIR}/windows/entry.cpp")
