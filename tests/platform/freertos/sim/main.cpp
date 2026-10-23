// SPDX-License-Identifier: GPL-3.0-or-later
// The FreeRTOS simulator program: Catch2 run in a FreeRTOS task, over the real platform layer
// (libs/platform/freertos) on the kernel's POSIX simulator port: a Linux process whose tasks are
// the kernel's own, with lwIP's UDP sockets over its loopback interface and littlefs on a RAM
// block device. The scheduler owns the process once it has started, so the tests run as a
// task, after the network stack and the file system are up, and the task ends the process
// with Catch2's exit code.
//
// Command-line arguments are Catch2's:  adl_freertos_tests "[conformance]" --durations yes

#include "freertos/environment.hpp"
#include "ram_file_system.hpp"

#include <lwip/tcpip.h>

#include <FreeRTOS.h>
#include <catch2/catch_session.hpp>
#include <semphr.h>
#include <task.h>

#include <cstdio>
#include <cstdlib>
#include <memory>

extern "C" void adl_freertos_assert_failed(const char* file, int line)
{
    std::fprintf(stderr, "FreeRTOS assertion failed at %s:%d\n", file, line);
    std::fflush(stderr);
    std::abort();
}

// The kernel asks the application for the memory of its idle task only when it was not
// configured to provide it (configKERNEL_PROVIDED_STATIC_MEMORY); nothing else to supply.

namespace
{

struct Arguments
{
    int argc;
    char** argv;
};

// The block device: 4 KB blocks, 8 MB, the same size as the Zephyr program's storage.
constexpr std::size_t block_size = 4096;
constexpr std::size_t block_count = 2048;

// Words of stack for the task Catch2 runs in; the 615A engine tests build large buffers there.
constexpr configSTACK_DEPTH_TYPE test_stack_words = 262144;

void stack_is_up(void* done)
{
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(done));
}

int run(const Arguments& arguments)
{
    // lwIP's thread, its loopback interface and 127.0.0.1.
    SemaphoreHandle_t up = xSemaphoreCreateBinary();
    tcpip_init(&stack_is_up, up);
    xSemaphoreTake(up, portMAX_DELAY);
    vSemaphoreDelete(up);

    adl::test::RamFileSystem file_system(block_size, block_count);
    if (!file_system.ok())
    {
        std::fprintf(stderr, "the RAM file system could not be made and mounted\n");
        return 2;
    }
    adl::platform::freertos::Environment product;
    product.file_system = file_system.get();
    product.network_task_priority = TCPIP_THREAD_PRIO;
    adl::platform::freertos::set_environment(product);

    Catch::Session session;
    return session.run(arguments.argc, arguments.argv);
}

void test_task(void* argument)
{
    const int status = run(*static_cast<const Arguments*>(argument));
    std::fflush(stdout);
    // The process ends here; the scheduler has no way back to main.
    std::exit(status);
}

} // namespace

int main(int argc, char** argv)
{
    static Arguments arguments{argc, argv};
    if (xTaskCreate(&test_task, "tests", test_stack_words, &arguments, 2, nullptr) != pdPASS)
    {
        std::fprintf(stderr, "no task for the tests\n");
        return 2;
    }
    vTaskStartScheduler();
    return 1;
}
