#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#if __LINUX_ARM_ARCH__ < 6
#error SMP not supported on pre-ARMv6 CPUs
#endif

#include <asm/processor.h>

extern int msm_krait_need_wfe_fixup;

#define ALT_SMP(smp, up)					\
	"9998:	" smp "\n"					\
	"	.pushsection \".alt.smp.init\", \"a\"\n"	\
	"	.long	9998b\n"				\
	"	" up "\n"					\
	"	.popsection\n"

#ifdef CONFIG_THUMB2_KERNEL
#define SEV		ALT_SMP("sev.w", "nop.w")
#define WFE()		ALT_SMP(		\
	"wfe.w",				\
	"nop.w"					\
)
#else
#define SEV		ALT_SMP("sev", "nop")
#define WFE()		ALT_SMP("wfe", "nop")
#endif

#ifdef CONFIG_MSM_KRAIT_WFE_FIXUP
#define WFE_SAFE(fixup, tmp) 				\
"	mrs	" tmp ", cpsr\n"			\
"	cmp	" fixup ", #0\n"			\
"	wfeeq\n"					\
"	beq	10f\n"					\
"	cpsid   f\n"					\
"	mrc	p15, 7, " fixup ", c15, c0, 5\n"	\
"	bic	" fixup ", " fixup ", #0x10000\n"	\
"	mcr	p15, 7, " fixup ", c15, c0, 5\n"	\
"	isb\n"						\
"	wfe\n"						\
"	orr	" fixup ", " fixup ", #0x10000\n"	\
"	mcr	p15, 7, " fixup ", c15, c0, 5\n"	\
"	isb\n"						\
"10:	msr	cpsr_cf, " tmp "\n"
#else
#define WFE_SAFE(fixup, tmp)	"	wfe\n"
#endif

static inline void dsb_sev(void)
{
#if __LINUX_ARM_ARCH__ >= 7
	__asm__ __volatile__ (
		"dsb\n"
#ifndef CONFIG_HTC_WFE_DISABLE
		SEV
#endif
	);
#else
	__asm__ __volatile__ (
		"mcr p15, 0, %0, c7, c10, 4\n"
		SEV
		: : "r" (0)
	);
#endif
}


#define arch_spin_unlock_wait(lock) \
	do { while (arch_spin_is_locked(lock)) cpu_relax(); } while (0)

#define arch_spin_lock_flags(lock, flags) arch_spin_lock(lock)

static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	unsigned long tmp, flags = 0;
	u32 newval;
	arch_spinlock_t lockval;

	__asm__ __volatile__(
"1:	ldrex	%0, [%3]\n"
"	add	%1, %0, %4\n"
"	strex	%2, %1, [%3]\n"
"	teq	%2, #0\n"
"	bne	1b"
	: "=&r" (lockval), "=&r" (newval), "=&r" (tmp)
	: "r" (&lock->slock), "I" (1 << TICKET_SHIFT)
	: "cc");

	while (lockval.tickets.next != lockval.tickets.owner) {
		if (msm_krait_need_wfe_fixup) {
			local_save_flags(flags);
			local_fiq_disable();
			__asm__ __volatile__(
			"mrc	p15, 7, %0, c15, c0, 5\n"
			: "=r" (tmp)
			:
			: "cc");
			tmp &= ~(0x10000);
			__asm__ __volatile__(
			"mcr	p15, 7, %0, c15, c0, 5\n"
			:
			: "r" (tmp)
			: "cc");
			isb();
		}
#ifndef CONFIG_HTC_WFE_DISABLE
		wfe();
#endif
		if (msm_krait_need_wfe_fixup) {
			tmp |= 0x10000;
			__asm__ __volatile__(
			"mcr	p15, 7, %0, c15, c0, 5\n"
			:
			: "r" (tmp)
			: "cc");
			isb();
			local_irq_restore(flags);
		}
		lockval.tickets.owner = ACCESS_ONCE(lock->tickets.owner);
	}

	smp_mb();
}

static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	unsigned long tmp;
	u32 slock;

	__asm__ __volatile__(
"	ldrex	%0, [%2]\n"
"	subs	%1, %0, %0, ror #16\n"
"	addeq	%0, %0, %3\n"
"	strexeq	%1, %0, [%2]"
	: "=&r" (slock), "=&r" (tmp)
	: "r" (&lock->slock), "I" (1 << TICKET_SHIFT)
	: "cc");

	if (tmp == 0) {
		smp_mb();
		return 1;
	} else {
		return 0;
	}
}

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	smp_mb();
	lock->tickets.owner++;
	dsb_sev();
}

static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	struct __raw_tickets tickets = ACCESS_ONCE(lock->tickets);
	return tickets.owner != tickets.next;
}

static inline int arch_spin_is_contended(arch_spinlock_t *lock)
{
	struct __raw_tickets tickets = ACCESS_ONCE(lock->tickets);
	return (tickets.next - tickets.owner) > 1;
}
#define arch_spin_is_contended	arch_spin_is_contended


static inline void arch_write_lock(arch_rwlock_t *rw)
{
	unsigned long tmp, fixup = msm_krait_need_wfe_fixup;

	__asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	teq	%0, #0\n"
"	beq	2f\n"
#ifndef CONFIG_HTC_WFE_DISABLE
	WFE_SAFE("%1", "%0")
#endif
"2:\n"
"	strexeq	%0, %3, [%2]\n"
"	teq	%0, #0\n"
"	bne	1b"
	: "=&r" (tmp), "+r" (fixup)
	: "r" (&rw->lock), "r" (0x80000000)
	: "cc");

	smp_mb();
}

static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	unsigned long tmp;

	__asm__ __volatile__(
"	ldrex	%0, [%1]\n"
"	teq	%0, #0\n"
"	strexeq	%0, %2, [%1]"
	: "=&r" (tmp)
	: "r" (&rw->lock), "r" (0x80000000)
	: "cc");

	if (tmp == 0) {
		smp_mb();
		return 1;
	} else {
		return 0;
	}
}

static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	smp_mb();

	__asm__ __volatile__(
	"str	%1, [%0]\n"
	:
	: "r" (&rw->lock), "r" (0)
	: "cc");

	dsb_sev();
}

#define arch_write_can_lock(x)		((x)->lock == 0)

static inline void arch_read_lock(arch_rwlock_t *rw)
{
	unsigned long tmp, tmp2, fixup = msm_krait_need_wfe_fixup;

	__asm__ __volatile__(
"1:	ldrex	%0, [%3]\n"
"	adds	%0, %0, #1\n"
"	strexpl	%1, %0, [%3]\n"
"	bpl	2f\n"
#ifndef CONFIG_HTC_WFE_DISABLE
	WFE_SAFE("%2", "%0")
#endif
"2:\n"
"	rsbpls	%0, %1, #0\n"
"	bmi	1b"
	: "=&r" (tmp), "=&r" (tmp2), "+r" (fixup)
	: "r" (&rw->lock)
	: "cc");

	smp_mb();
}

static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	unsigned long tmp, tmp2;

	smp_mb();

	__asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	sub	%0, %0, #1\n"
"	strex	%1, %0, [%2]\n"
"	teq	%1, #0\n"
"	bne	1b"
	: "=&r" (tmp), "=&r" (tmp2)
	: "r" (&rw->lock)
	: "cc");

	if (tmp == 0)
		dsb_sev();
}

static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	unsigned long tmp, tmp2 = 1;

	__asm__ __volatile__(
"	ldrex	%0, [%2]\n"
"	adds	%0, %0, #1\n"
"	strexpl	%1, %0, [%2]\n"
	: "=&r" (tmp), "+r" (tmp2)
	: "r" (&rw->lock)
	: "cc");

	smp_mb();
	return tmp2 == 0;
}

#define arch_read_can_lock(x)		((x)->lock < 0x80000000)

#define arch_read_lock_flags(lock, flags) arch_read_lock(lock)
#define arch_write_lock_flags(lock, flags) arch_write_lock(lock)

#define arch_spin_relax(lock)	cpu_relax()
#define arch_read_relax(lock)	cpu_relax()
#define arch_write_relax(lock)	cpu_relax()

#endif 
