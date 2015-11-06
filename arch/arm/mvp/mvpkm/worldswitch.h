/* ********************************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
 * -- VMware Confidential
 * ********************************************************************/

/**
 *  @file
 *
 *  @brief Definition of the world switch page
 *
 *  Two pages are maintained to facilitate switching from the vmx to
 *  the monitor - a data and code page. The data page contains:
 *   - the necessary information about itself (its MPN, KVA, ...)
 *   - the saved register file of the other world (including some cp15 regs)
 *   - some information about the monitor's address space (the monVA member)
 *     that needed right after the w.s before any communication channels
 *     could have been established
 *   - a world switch related L2 table of the monitor -- this could be
 *     elsewhere.
 *
 *   The code page contains:
 *   - the actual switching code that saves/restores the registers
 *
 *   The world switch data page is mapped into the user, kernel, and the monitor
 *   address spaces. In case of the user and monitor spaces the global variable
 *   wsp points to the world switch page (in the vmx and the monitor
 *   respectively). The kernel address of the world switch page is saved on
 *   the page itself: wspHKVA.
 *
 *   The kernel virtual address for both code and data pages is mapped into
 *   the monitor's space temporarily at the time of the actual switch. This is
 *   needed to provide a stable code and data page while the L1 page table
 *   base is changing. As the monitor does not need the world switch data page
 *   at its KVA for its internal operation, that map is severed right after the
 *   switching to the monitor and re-established before switching back.
 */
#ifndef _WORLDSWITCH_H
#define _WORLDSWITCH_H

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

/**
 *  @brief Area for saving the monitor/kernel register files.
 *
 *  The order of the registers in this structure was designed to
 *  facilitate the organization of the switching code. For example
 *  all Supervisor Mode registers are grouped together allowing the
 * @code
 *      switch to svc,
 *      stm old svc regs
 *      ldm new svc regs
 * @endcode
 *  code to work using a single base register for both the store and
 *  load area.
 */

#ifndef __ASSEMBLER__
typedef void (*SwitchToMonitor)(void *regSave);
typedef void (*SwitchToUser)(void *regSaveEnd);
#endif

#ifdef __aarch64__
#include "worldswitch64.h"
#else
#include "worldswitch32.h"
#endif

#endif /* _WORLDSWITCH_H */
