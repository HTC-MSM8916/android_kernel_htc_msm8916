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
 * @brief Inline stubs for ARM assembler instructions.
 */

#ifndef _ARM64_INLINE_H_
#define _ARM64_INLINE_H_

/*
 * Compiler specific include - we get the actual inline assembler macros here.
 */
#include "arm64_gcc_inline.h"

/*
 * Some non-compiler specific helper functions for inline assembler macros
 * included above.
 */

/**
 * @brief Predicate giving whether interrupts are currently enabled
 *
 * @return TRUE if enabled, FALSE otherwise
 */
static inline _Bool
ARM_InterruptsEnabled(void)
{
	return !(ARM_READO_SYSTEM_REG_32(daif) & ARM_PSR_I);
}

/**
 * @brief Read current TTBR0 base machine address
 *
 * @return machine address given by translation table base register 0
 */
static inline MA
ARM_ReadTTBase0(void)
{
	/*TODO: part of monitor porting */
	ASSERT(false);
	return 0;
}

/**
 * @brief Read VFP/Adv.SIMD Extension System Register
 *
 * @param specReg which VFP/Adv. SIMD Extension System Register
 *
 * @return Read value
 */
static inline uint32
ARM_ReadVFPSystemRegister(uint8 specReg)
{
	/*TODO: part of monitor porting */
	ASSERT(false);
	return 0;
}

/**
 * @brief Write to VFP/Adv.SIMD Extension System Register
 *
 * @param specReg which VFP/Adv. SIMD Extension System Register
 * @param value desired value to be written to the System Register
 */
static inline void
ARM_WriteVFPSystemRegister(uint8 specReg,
			   uint32 value)
{
	/*TODO: part of monitor porting */
	ASSERT(false);
	return;
}

/**
 * @brief Read current sytem 32 bits register
 *
 * @param reg which System Register to read
 *
 * @return value of system register reg
 */
static inline uint64
ARM_ReadSystemRegister(uint8 reg)
{
	uint64 value;
	switch (reg) {
	case ARM_MVP_PERF_PMNC:
		ARM_READ_SYSTEM_REG(PMCR_EL0, value);
		break;
	case ARM_MVP_PERF_PMCNT:
		ARM_READ_SYSTEM_REG(PMCNTENSET_EL0, value);
		break;
	case ARM_MVP_VBAR:
		ARM_READ_SYSTEM_REG(VBAR_EL1, value);
		break;
	default:
		NOT_IMPLEMENTED();
		break;
	}

	return value;
}

/**
 * @brief Write system 32 bits register
 *
 * @param reg which System Register to write
 * @param value desired value to be written to the register
 */
static inline void
ARM_WriteSystemRegister(uint8 reg,
			uint32 value)
{
	switch (reg) {
	case ARM_MVP_PERF_PMNC:
		ARM_WRITE_SYSTEM_REG(PMCR_EL0, value);
		break;
	case ARM_MVP_PERF_PMCNT:
		ARM_WRITE_SYSTEM_REG(PMCNTENSET_EL0, value);
		break;
	case ARM_MVP_VBAR:
		ARM_WRITE_SYSTEM_REG(VBAR_EL1, value);
		break;
	default:
		NOT_IMPLEMENTED();
		break;
	}
}
#endif /* ifndef _ARM64_INLINE_H_ */
