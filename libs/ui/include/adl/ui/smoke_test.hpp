// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class QQuickWindow;

namespace adl::ui
{

// The flag that asks a program to prove it can show its window and then leave.
inline constexpr const char* smoke_test_flag = "--smoke-test";

// True when the program was started with --smoke-test.
[[nodiscard]] bool smoke_test_requested();

// For a program started with --smoke-test: when `window` has drawn its first frame the program
// prints "smoke-test: first frame drawn" and exits with code 0. A program that never shows its
// window never gets there, which the caller (the system smoke test) sees as a timeout, and one
// whose QML fails to load exits 1 before this is reached. Does nothing without the flag.
void install_smoke_test(QQuickWindow* window);

} // namespace adl::ui
