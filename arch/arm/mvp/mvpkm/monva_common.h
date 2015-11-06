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
 *  @file
 *
 *  @brief Constant definitions that describing the monitor memory layout
 *         (common to both LPV and VE monitors).
 *
 */

#ifndef _MONVA_COMMON_H_
#define _MONVA_COMMON_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_HOSTUSER
#define INCLUDE_ALLOW_GUESTUSER
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#include "mmu_defs.h"
#include "mmu_types.h"
#ifdef CONFIG_ARM64
#include "arm64_types.h"
#endif

/*
 * The monitor occupies a hole in the guest virtual address space.
 * The following macros define that hole.
 */

#define MONITOR_VA_START       ((MVA)0xE8000000)
#define MONITOR_VA_LEN         0x03000000

#ifndef __aarch64__
/*
 * 32 bits: Worldswitch page gets mapped right after the stack guard.
 */
#define MONITOR_VA_WORLDSWITCH \
	((MVA)(MONITOR_VA_START + 3 * PAGE_SIZE))
#else
/*
 * 64 bits: Worldswitch page gets mapped right after the stack.
 */
#define MONITOR_VA_WORLDSWITCH \
	((MVA)(MONITOR_VA_START + 2 * PAGE_SIZE))
#endif

#ifdef IN_MONITOR
#define MONITOR_VA_UART \
	(MONITOR_VA_WORLDSWITCH + WSP_PAGE_COUNT * PAGE_SIZE)
#endif

/**
 * @brief Type of physmem region mapping that we want the VMX to know about.
 *        Helps to identify Guest page allocations.
 */
typedef enum {
	MEMREGION_MAINMEM = 1,
	MEMREGION_MODULE = 2,
	MEMREGION_WSP = 3,
	MEMREGION_MONITOR_MISC = 4,
	MEMREGION_DEFAULT = 0
} PACKED PhysMem_RegionType;

typedef struct MonVA {		/* Note that this struct is VE only */
	MA  l2BaseMA;		/**< MA of monitor L2 page table page */
	MVA excVec;		/**< Monitor exception vector virtual address */
} MonVA;

/**
 * @brief Monitor VA mapping type, device or memory.
 *
 * These values are used to index HMAIR0 in the VE monitor - do not change
 * without making the required update to HMAIR0.
 */
typedef enum {
	MVA_MEMORY = 0,
	MVA_DEVICE = 1
} MVAType;

/**
 * @name Monitor types, used in VMX, Mvpkm and monitors.
 *
 * This is not a C enumeration, as we may want to use the values in CPP macros.
 *
 * @{
 */
#define MONITOR_TYPE_LPV        0
#define MONITOR_TYPE_VE         1
#define MONITOR_TYPE_LPV64      2
#define MONITOR_TYPE_UNKNOWN  0xf

typedef uint32 MonitorType;
/*@}*/

#endif
