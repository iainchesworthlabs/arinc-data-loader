# The configuration of the FreeRTOS simulator program: FreeRTOSConfig.h, the options of lwIP
# (lwipopts.h) and the compiler adaptation lwIP asks the port for (arch/cc.h).
#
# They are headers of preprocessor definitions, and this repository's C++ has none
# (docs/platform-isolation.md: what the build hands a program arrives as a compile definition or a
# header CMake generates). So the headers are written here, as text, and CMake generates them into
# the build directory; the C++ and C sources never hold them. A product has its own
# FreeRTOSConfig.h and lwipopts.h; what this platform layer needs from them is listed in
# docs/building.md, and these are the simulator's.

set(ADL_FREERTOS_CONFIG_TEXT [=[
// The kernel: pre-emptive, 1 ms tick, dynamic and static allocation both on (a thread made
// with a stack of the caller's uses the static calls).
#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 8
#define configMINIMAL_STACK_SIZE 1024
#define configMAX_TASK_NAME_LEN 16
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configQUEUE_REGISTRY_SIZE 0
#define configUSE_QUEUE_SETS 0
#define configUSE_TIMERS 0
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configUSE_MALLOC_FAILED_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 0
#define configUSE_TRACE_FACILITY 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 1
#define configKERNEL_PROVIDED_STATIC_MEMORY 1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0
#define configUSE_POSIX_ERRNO 0

// What the platform layer and lwIP's port use of the API.
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_eTaskGetState 1

// A failed assertion stops the program with a message the test run shows.
#ifdef __cplusplus
extern "C" {
#endif
void adl_freertos_assert_failed(const char* file, int line);
#ifdef __cplusplus
}
#endif
#define configASSERT(x) ((x) ? (void)0 : adl_freertos_assert_failed(__FILE__, __LINE__))
]=])

set(ADL_LWIP_OPTIONS_TEXT [=[
// A threaded stack with sockets, UDP over IPv4 and the loopback interface only: the simulator
// has no link to put an Ethernet interface on, and the conformance suite talks to 127.0.0.1.
#define NO_SYS 0
#define LWIP_SOCKET 1
#define LWIP_NETCONN 1
#define LWIP_COMPAT_SOCKETS 0
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
#define LWIP_SOCKET_SELECT 0
#define LWIP_SOCKET_POLL 1
#define LWIP_TIMEVAL_PRIVATE 0
#define LWIP_ERRNO_STDINCLUDE 1
#define LWIP_TCP 0
#define LWIP_UDP 1
#define LWIP_RAW 0
#define LWIP_ICMP 1
#define LWIP_IPV4 1
#define LWIP_IPV6 0
#define LWIP_DNS 0
#define LWIP_DHCP 0
#define LWIP_AUTOIP 0
#define LWIP_IGMP 0
#define LWIP_ARP 0
#define LWIP_ETHERNET 0
#define LWIP_STATS 0
#define LWIP_NETIF_LOOPBACK 1
#define LWIP_HAVE_LOOPIF 1
#define LWIP_LOOPBACK_MAX_PBUFS 256
#define LWIP_TCPIP_CORE_LOCKING 1
#define LWIP_NETCONN_SEM_PER_THREAD 0

// The stack's task: the loader's threads run below it (the platform is told its priority).
#define TCPIP_THREAD_NAME "tcpip"
#define TCPIP_THREAD_STACKSIZE 16384
#define TCPIP_THREAD_PRIO (configMAX_PRIORITIES - 2)
#define TCPIP_MBOX_SIZE 128
#define DEFAULT_UDP_RECVMBOX_SIZE 256
#define DEFAULT_RAW_RECVMBOX_SIZE 4
#define DEFAULT_ACCEPTMBOX_SIZE 4

// Memory: a 65507 byte datagram is one pbuf, twice (the sender's and the loopback copy), and the
// conformance suite has several in flight.
#define MEM_ALIGNMENT 8
#define MEM_SIZE (8 * 1024 * 1024)
#define MEMP_NUM_UDP_PCB 64
#define MEMP_NUM_NETCONN 64
#define MEMP_NUM_NETBUF 256
#define MEMP_NUM_PBUF 256
#define MEMP_NUM_TCPIP_MSG_API 64
#define MEMP_NUM_TCPIP_MSG_INPKT 256
#define PBUF_POOL_SIZE 128
#define PBUF_POOL_BUFSIZE 1536

// Socket options the platform layer sets.
#define SO_REUSE 1
#define LWIP_SO_RCVBUF 1
#define RECV_BUFSIZE_DEFAULT 8388608
#define IP_SOF_BROADCAST 1
#define IP_SOF_BROADCAST_RECV 1
]=])

# What lwIP asks of the port's compiler header (the host's C library supplies the rest).
set(ADL_LWIP_CC_TEXT [=[
#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define LWIP_PLATFORM_DIAG(x) ((void)printf x)
#define LWIP_PLATFORM_ASSERT(x) (printf("lwIP assertion failed: %s at %s:%d\n", x, __FILE__, __LINE__), abort())
#define LWIP_RAND() ((u32_t)rand())
]=])

# Writes the text to <path> as a header, when the build is generated.
function(adl_generate_header path text)
    file(GENERATE OUTPUT "${path}" CONTENT "#pragma once\n// Written by CMake from tests/platform/freertos/config.cmake. Do not edit.\n${text}")
endfunction()
