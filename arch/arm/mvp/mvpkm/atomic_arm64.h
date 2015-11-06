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
 * @brief bus-atomic operators, ARM64 implementation.
 * Do not include directly, include 'atomic.h' instead.
 * Memory where the atomics reside must be shared.
 *
 */

#ifndef _ATOMIC_ARM64_H
#define _ATOMIC_ARM64_H

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#define INCLUDE_ALLOW_HOSTUSER
#define INCLUDE_ALLOW_GUESTUSER
#include "include_check.h"

#include "mvp_assert.h"

/**
 * @brief Atomic Add
 * @param atm atomic cell to operate on
 * @param modval value to apply to atomic cell
 * @return the original value of 'atm'
 */
#define ATOMIC_ADDO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, add)

/**
 * @brief Atomic Add
 * @param atm atomic cell to operate on
 * @param modval value to apply to atomic cell
 * @return nothing
 */
#define ATOMIC_ADDV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, add)

/**
 * @brief Atomic And
 * @param atm atomic cell to operate on
 * @param modval value to apply to atomic cell
 * @return the original value of 'atm'
 */
#define ATOMIC_ANDO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, and)

/**
 * @brief Atomic And
 * @param atm atomic cell to operate on
 * @param modval value to apply to atomic cell
 * @return nothing
 */
#define ATOMIC_ANDV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, and)

/**
 * @brief Retrieve an atomic value. A normal load is atomic if aligned
 * @param atm atomic cell to operate on
 * @return the value of 'atm'
 */
#define ATOMIC_GETO(atm) ({							\
	typeof((atm).atm_Normal) _oldval;					\
										\
	switch (sizeof(_oldval)) {						\
	case 4:									\
		_oldval = ATOMIC_SINGLE_COPY_READ32(&((atm).atm_Volatl));	\
		break;								\
	case 8:									\
		_oldval = ATOMIC_SINGLE_COPY_READ64(&((atm).atm_Volatl));	\
		break;								\
	default:								\
		FATAL();							\
	}									\
	_oldval;								\
})

/**
 * @brief Atomic Or
 * @param atm atomic cell to operate on
 * @param modval value to apply to atomic cell
 * @return the original value of 'atm'
 */
#define ATOMIC_ORO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, orr)

/**
 * @brief Atomic Or
 * @param atm atomic cell to operate on
 * @param modval value to apply to atomic cell
 * @return nothing
 */
#define ATOMIC_ORV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, orr)

/**
 * @brief Atomic Conditional Write, ie,
 *        set 'atm' to 'newval' iff it was 'oldval'.
 * @param atm atomic cell to operate on
 * @param newval value to possibly write to atomic cell
 * @param oldval value that atomic cell must equal
 * @return 0 if failed; 1 if successful
 */
#define ATOMIC_SETIF(atm, newval, oldval) ({				\
	int _failed;							\
	typeof((atm).atm_Normal) _newval = newval;			\
	typeof((atm).atm_Normal) _oldval = oldval;			\
									\
	switch (sizeof(_newval)) {					\
	case 4:								\
		asm volatile (" // atomic 32 conditional set\n"		\
"1:	ldxr    %w0, [%1]\n"						\
"	cmp	%w0, %w2\n"						\
"	mov	%w0, #1\n"						\
"	B.ne	2f\n"							\
"	stxr	%w0, %w3, [%1]\n"					\
"	cbnz	%w0, 1b\n"						\
"2:"									\
	: "=&r" (_failed)						\
	: "r"   (&((atm).atm_Volatl)), "r" (_oldval), "r" (_newval)	\
	: "cc", "memory");						\
		break;							\
	case 8:								\
		asm volatile (" // atomic 64 conditional set\n"		\
"1:	ldxr    %0, [%1]\n"						\
"	cmp	%0, %2\n"						\
"	mov	%w0, #1\n"						\
"	B.ne	2f\n"							\
"	stxr	%w0, %3, [%1]\n"					\
"	cbnz	%w0, 1b\n"						\
"2:"									\
	: "=&r" (_failed)						\
	: "r"   (&((atm).atm_Volatl)), "r" (_oldval), "r" (_newval)	\
	: "cc", "memory");						\
		break;							\
	default:							\
		FATAL();						\
	}								\
	!_failed;							\
})


/**
 * @brief Atomic Write (unconditional)
 * @param atm atomic cell to operate on
 * @param newval value to write to atomic cell
 * @return the original value of 'atm'
 */
#define ATOMIC_SETO(atm, newval) ({				\
	int _tmp;						\
	typeof((atm).atm_Normal) _oldval;			\
	typeof((atm).atm_Normal) _newval = newval;		\
								\
	switch (sizeof(_newval)) {				\
	case 4:							\
		asm volatile (" // atomic 32 set\n"		\
"1:	ldxr    %w0, [%2]\n"					\
"	stxr	%w1, %w3, [%2]\n"				\
"	cbnz	%w1, 1b\n"					\
	: "=&r" (_oldval), "=&r" (_tmp)				\
	: "r"   (&((atm).atm_Volatl)), "r" (_newval)		\
	: "memory");						\
		break;						\
	case 8:							\
		asm volatile (" // atomic 64 set\n"		\
"1:	ldxr    %0, [%2]\n"					\
"	stxr	%w1, %3, [%2]\n"				\
"	cbnz	%w1, 1b\n"					\
	: "=&r" (_oldval), "=&r" (_tmp)				\
	: "r"   (&((atm).atm_Volatl)), "r" (_newval)		\
	: "memory");						\
		break;						\
	default:						\
		FATAL();					\
	}							\
	_oldval;						\
})

/**
 * @brief Atomic Write (unconditional)
 * @param atm atomic cell to operate on
 * @param newval value to write to atomic cell
 * @return nothing
 */
#define ATOMIC_SETV(atm, newval) ATOMIC_SETO((atm), (newval))

/**
 * @brief Atomic Substract
 * @param atm atomic cell to operate on
 * @param modval value to apply to atomic cell
 * @return the original value of 'atm'
 */
#define ATOMIC_SUBO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, sub)

/**
 * @brief Atomic Substract
 * @param atm atomic cell to operate on
 * @param modval value to apply to atomic cell
 * @return nothing
 */
#define ATOMIC_SUBV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, sub)

/**
 * @brief Atomic Generic Binary Operation
 * @param atm atomic cell to operate on
 * @param modval value to apply to atomic cell
 * @param op ARM instruction that doesn't update cpu flags (add, and, orr, etc)
 * @return the original value of 'atm'
 */
#define ATOMIC_OPO_PRIVATE(atm, modval, op) ({			\
	int _tmp;						\
	typeof((atm).atm_Normal) _modval = modval;		\
	typeof((atm).atm_Normal) _oldval;			\
	typeof((atm).atm_Normal) _newval;			\
								\
	switch (sizeof(_newval)) {				\
	case 4:							\
		asm volatile (" // atomic 32 op output\n"	\
"1:	ldxr    %w0, [%3]\n"					\
	#op "	%w1, %w0, %w4\n"				\
"	stxr	%w2, %w1, [%3]\n"				\
"	cbnz	%w2, 1b\n"					\
	: "=&r" (_oldval), "=&r" (_newval), "=&r" (_tmp)	\
	: "r"   (&((atm).atm_Volatl)), "Ir"   (_modval)		\
	: "memory");						\
		break;						\
	case 8:							\
		asm volatile (" // atomic 64 op output\n"	\
"1:	ldxr    %0, [%3]\n"					\
	#op "	%1, %0, %4\n"					\
"	stxr	%w2, %1, [%3]\n"				\
"	cbnz	%w2, 1b\n"					\
	: "=&r" (_oldval), "=&r" (_newval), "=&r" (_tmp)	\
	: "r"   (&((atm).atm_Volatl)), "Ir"   (_modval)		\
	: "memory");						\
		break;						\
	default:						\
		FATAL();					\
	}							\
	_oldval;						\
})

/**
 * @brief Atomic Generic Binary Operation
 * @param atm atomic cell to operate on
 * @param modval value to apply to atomic cell
 * @param op ARM instruction (add, and, orr, etc)
 * @return nothing
 */
#define ATOMIC_OPV_PRIVATE(atm, modval, op) do {		\
	int _failed;						\
	typeof((atm).atm_Normal) _modval = modval;		\
	typeof((atm).atm_Normal) _sample;			\
								\
	switch (sizeof(_modval)) {				\
	case 4:							\
		asm volatile (" // atomic 32 op void\n"		\
"1:	ldxr    %w0, [%2]\n"					\
	#op "	%w0, %w0, %w3\n"				\
"	stxr	%w1, %w0, [%2]\n"				\
"	cbnz	%w1, 1b\n"					\
	: "=&r" (_sample), "=&r" (_failed)			\
	: "r"   (&((atm).atm_Volatl)), "Ir"   (_modval)		\
	: "memory");						\
		break;						\
	case 8:							\
		asm volatile (" // atomic 64 op void\n"		\
"1:	ldxr    %0, [%2]\n"					\
	#op "	%0, %0, %3\n"					\
"	stxr	%w1, %0, [%2]\n"				\
"	cbnz	%w1, 1b\n"					\
	: "=&r" (_sample), "=&r" (_failed)			\
	: "r"   (&((atm).atm_Volatl)), "Ir"   (_modval)		\
	: "memory");						\
		break;						\
	default:						\
		FATAL();					\
	}							\
} while (0)


/**
 * According to ARM DDI 0487A.b B2-79, all reads generated by load instructions that load
 * a single general-purpose register and that are aligned to the size of the read in that
 * instruction are single-copy atomic. And all writes generated by store instructions that
 * store a single general-purpose register and that are aligned to the size of the write in
 * that instruction are single-copy atomic.
 *
 * So we don't need any assembly for the following macros.
 */

/**
 * @brief Single-copy atomic word write.
 *
 * @param p word aligned location to write to
 * @param val word-sized value to write to p
 */
#define ATOMIC_SINGLE_COPY_WRITE32(p, val) do {	\
	ASSERT(sizeof(val) == 4);		\
	ASSERT((MVA)(p) % sizeof(val) == 0);	\
	*(uint32*)(p) = (val);			\
} while (0)

/**
 * @brief Single-copy atomic word read.
 *
 * @param p word aligned location to read from
 *
 * @return word-sized value from p
 */
#define ATOMIC_SINGLE_COPY_READ32(p) ({		\
	ASSERT((MVA)(p) % sizeof(uint32) == 0);	\
	(uint32) *(p);				\
})

/**
 * @brief Single-copy atomic double word write.
 *
 * @param p word aligned location to write to
 * @param val word-sized value to write to p
 *
 */
#define ATOMIC_SINGLE_COPY_WRITE64(p, val) do {	\
	ASSERT(sizeof(val) == 8);		\
	ASSERT((MVA)(p) % sizeof(val) == 0);	\
	*(uint64 *)(p) = (val);			\
} while (0)

/**
 * @brief Single-copy atomic double word read.
 *
 * @param p double word aligned location to read from
 *
 * @return double word-sized value from p
 */

#define ATOMIC_SINGLE_COPY_READ64(p) ({		\
	ASSERT((MVA)(p) % sizeof(uint64) == 0);	\
	(uint64) *(p);				\
})
#endif
