// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// lwIP's operating system types, on FreeRTOS: the port header lwIP asks for as
// <arch/sys_arch.h>. Included by lwIP's C and by sys_arch_freertos.cpp, so it holds
// typedefs and nothing that needs a language test.

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

typedef SemaphoreHandle_t sys_sem_t;
typedef SemaphoreHandle_t sys_mutex_t;
typedef QueueHandle_t sys_mbox_t;
typedef TaskHandle_t sys_thread_t;
typedef unsigned int sys_prot_t;
