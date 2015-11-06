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
 * @brief Facilities for uid -> kuid changes
 */

#ifndef _NSUID_H
#define _NSUID_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

/*
 * v3.5 introduced changes to account for different namespaces,
 * differentiating between kernel and user uids and gids.
 *
 * In older kernel versions, we use same uids/gids.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0)

/* New types */
typedef uid_t kuid_t;
typedef gid_t kgid_t;

/* equality */
#define uid_eq(uid1, uid2) ((uid1) == (uid2))
#define gid_eq(gid1, gid2) ((gid1) == (gid2))

/* translate a kuid to the uid in the right namespace */
#define from_kuid(to_namespace, kuid) (kuid)

/* constants */
#define GLOBAL_ROOT_UID 0
#define INVALID_UID -1
#define INVALID_GID -1
#endif

#endif
