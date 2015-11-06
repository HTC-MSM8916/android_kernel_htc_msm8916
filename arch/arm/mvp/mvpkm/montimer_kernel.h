/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#line 5

/**
 * @file
 *
 * @brief The monitor-kernel socket interface kernel-only definitions.
 */

#ifndef _MONITOR_TIMER_KERNEL_H
#define _MONITOR_TIMER_KERNEL_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#include <linux/hrtimer.h>

/**
 * @brief Monitor Timer structure
 */
struct MonTimer {
	struct MvpkmVM *vm;         /**< Associated vm */
	struct hrtimer  timer;      /**< Linux timer */
};

void MonitorTimer_Setup(struct MvpkmVM *vm);
void MonitorTimer_Request(struct MonTimer *monTimer, uint64 when64);

#endif

