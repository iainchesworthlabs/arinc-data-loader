**The Zephyr and FreeRTOS platforms** (experimental): the platform layer on
Zephyr's native APIs (`k_*`, `fs_*`, `zsock_*`), as a Zephyr module
(`zephyr/module.yml`, Kconfig, CMake), and on FreeRTOS with lwIP's sockets
API and littlefs. Clock, memory, files, threads and priorities, UDP and
the message queue, with the same interfaces as the desktops. The platform
conformance suite and the 615A engine tests (224 test cases) run in CI on
Zephyr's `native_sim` board and on the FreeRTOS POSIX simulator port, in two new
jobs that are not required checks yet (`Zephyr (native_sim)` and `FreeRTOS
(POSIX simulator)`); no board has run either. The Zephyr platform layer was also
built with the Zephyr SDK's own GCC and picolibc for a Cortex-M3 and a smoke test
run in QEMU, and the whole test program (224 test cases) runs on a Cortex-A9 in QEMU with
that toolchain (`Zephyr (QEMU, SDK toolchain)`). The contract gains the status
`experimental` and `ADL_PLATFORM_PROJECT`, for a platform whose image a project of
its own builds, so the top-level project refuses `-DADL_TARGET_PLATFORM=freertos`
and `zephyr` and says where to build them. What was verified and what a
simulator cannot show is in
[docs/platform-abstraction.md](docs/platform-abstraction.md#the-freertos-and-zephyr-ports);
how to build them in [docs/building.md](docs/building.md#zephyr-and-freertos).
