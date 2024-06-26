/*
 * Copyright (c) 2000-2023 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 *
 */

#ifndef I386_CPU_DATA
#define I386_CPU_DATA

#include <mach_assert.h>
#include <machine/atomic.h>
#include <machine/monotonic.h>

#include <kern/assert.h>
#include <kern/kern_types.h>
#include <kern/mpqueue.h>
#include <kern/queue.h>
#include <kern/processor.h>
#include <kern/pms.h>
#include <pexpert/pexpert.h>
#include <mach/i386/thread_status.h>
#include <mach/i386/vm_param.h>
#include <i386/locks.h>
#include <i386/rtclock_protos.h>
#include <i386/pmCPU.h>
#include <i386/cpu_topology.h>
#include <i386/seg.h>
#include <i386/mp.h>

#if CONFIG_VMX
#include <i386/vmx/vmx_cpu.h>
#endif

#include <san/kcov_data.h>

#include <machine/pal_routines.h>

/*
 * Data structures referenced (anonymously) from per-cpu data:
 */
struct cpu_cons_buffer;
struct cpu_desc_table;
struct mca_state;
struct prngContext;

/*
 * Data structures embedded in per-cpu data:
 */
typedef struct rtclock_timer {
	mpqueue_head_t          queue;
	uint64_t                deadline;
	uint64_t                when_set;
	boolean_t               has_expired;
} rtclock_timer_t;

typedef struct {
	/* The 'u' suffixed fields store the double-mapped descriptor addresses */
	struct x86_64_tss       *cdi_ktssu;
	struct x86_64_tss       *cdi_ktssb;
	x86_64_desc_register_t  cdi_gdtu;
	x86_64_desc_register_t  cdi_gdtb;
	x86_64_desc_register_t  cdi_idtu;
	x86_64_desc_register_t  cdi_idtb;
	struct real_descriptor  *cdi_ldtu;
	struct real_descriptor  *cdi_ldtb;
	vm_offset_t             cdi_sstku;
	vm_offset_t             cdi_sstkb;
} cpu_desc_index_t;

typedef enum {
	TASK_MAP_32BIT,                 /* 32-bit user, compatibility mode */
	TASK_MAP_64BIT,                 /* 64-bit user thread, shared space */
} task_map_t;


/*
 * This structure is used on entry into the (uber-)kernel on syscall from
 * a 64-bit user. It contains the address of the machine state save area
 * for the current thread and a temporary place to save the user's rsp
 * before loading this address into rsp.
 */
typedef struct {
	addr64_t        cu_isf;         /* thread->pcb->iss.isf */
	uint64_t        cu_tmp;         /* temporary scratch */
	addr64_t        cu_user_gs_base;
} cpu_uber_t;

typedef uint16_t        pcid_t;
typedef uint8_t         pcid_ref_t;

#define CPU_RTIME_BINS (12)
#define CPU_ITIME_BINS (CPU_RTIME_BINS)

#define MAX_TRACE_BTFRAMES (16)
typedef struct {
	boolean_t pltype;
	int plevel;
	uint64_t plbt[MAX_TRACE_BTFRAMES];
} plrecord_t;

#if     DEVELOPMENT || DEBUG

typedef struct {
	int             vector;                 /* Vector number of interrupt */
	thread_t        curthread;              /* Current thread at the time of the interrupt */
	uint64_t        interrupted_pc;
	int             curpl;                  /* Current preemption level */
	int             curil;                  /* Current interrupt level */
	uint64_t        start_time_abs;
	uint64_t        duration;
	uint64_t        backtrace[MAX_TRACE_BTFRAMES];
} traptrace_entry_t;

#define TRAPTRACE_INVALID_INDEX (~0U)
#define DEFAULT_TRAPTRACE_ENTRIES_PER_CPU (16)
#define TRAPTRACE_MAX_ENTRIES_PER_CPU (256)
extern volatile int traptrace_enabled;
extern uint32_t traptrace_entries_per_cpu;
PERCPU_DECL(uint32_t, traptrace_next);
PERCPU_DECL(traptrace_entry_t * __unsafe_indexable, traptrace_ring);
#endif /* DEVELOPMENT || DEBUG */

/*
 * Per-cpu data.
 *
 * Each processor has a per-cpu data area which is dereferenced through the
 * current_cpu_datap() macro. For speed, the %gs segment is based here, and
 * using this, inlines provides single-instruction access to frequently used
 * members - such as get_cpu_number()/cpu_number(), and get_active_thread()/
 * current_thread().
 *
 * Cpu data owned by another processor can be accessed using the
 * cpu_datap(cpu_number) macro which uses the cpu_data_ptr[] array of per-cpu
 * pointers.
 */
typedef struct {
	pcid_t                  cpu_pcid_free_hint;
#define PMAP_PCID_MAX_PCID      (0x800)
	pcid_ref_t              cpu_pcid_refcounts[PMAP_PCID_MAX_PCID];
	pmap_t                  cpu_pcid_last_pmap_dispatched[PMAP_PCID_MAX_PCID];
} pcid_cdata_t;

typedef struct cpu_data {
	struct pal_cpu_data     cpu_pal_data;           /* PAL-specific data */
#define                         cpu_pd cpu_pal_data     /* convenience alias */
	struct cpu_data         *cpu_this;              /* pointer to myself */
	vm_offset_t             cpu_pcpu_base;
	thread_t                cpu_active_thread;
	thread_t                cpu_nthread;
	int                     cpu_number;             /* Logical CPU */
	void                    *cpu_int_state;         /* interrupt state */
	vm_offset_t             cpu_active_stack;       /* kernel stack base */
	vm_offset_t             cpu_kernel_stack;       /* kernel stack top */
	vm_offset_t             cpu_int_stack_top;
	volatile int            cpu_signals;            /* IPI events */
	volatile int            cpu_prior_signals;      /* Last set of events,
	                                                 * debugging
	                                                 */
	ast_t                   cpu_pending_ast;
	/*
	 * Note if rearranging fields:
	 * We want cpu_preemption_level on a different
	 * cache line than cpu_active_thread
	 * for optimizing mtx_spin phase.
	 */
	int                     cpu_interrupt_level;
	volatile int            cpu_preemption_level;
	volatile int            cpu_running;
#if !CONFIG_CPU_COUNTERS
	boolean_t               cpu_fixed_pmcs_enabled;
#endif /* !CONFIG_CPU_COUNTERS */
	rtclock_timer_t         rtclock_timer;
	volatile addr64_t       cpu_active_cr3 __attribute((aligned(64)));
	union {
		volatile uint32_t cpu_tlb_invalid;
		struct {
			volatile uint16_t cpu_tlb_invalid_local;
			volatile uint16_t cpu_tlb_invalid_global;
		};
	};
	uint64_t                cpu_ip_desc[2];
	volatile task_map_t     cpu_task_map;
	volatile addr64_t       cpu_task_cr3;
	addr64_t                cpu_kernel_cr3;
	volatile addr64_t       cpu_ucr3;
	volatile addr64_t       cpu_shadowtask_cr3;
	boolean_t               cpu_pagezero_mapped;
	cpu_uber_t              cpu_uber;
/* Double-mapped per-CPU exception stack address */
	uintptr_t               cd_estack;
	int                     cpu_xstate;
	int                     cpu_curtask_has_ldt;
	int                     cpu_curthread_do_segchk;
/* Address of shadowed, partially mirrored CPU data structures located
 * in the double mapped PML4
 */
	void                    *cd_shadow;
	union {
		volatile uint32_t cpu_tlb_invalid_count;
		struct {
			volatile uint16_t cpu_tlb_invalid_local_count;
			volatile uint16_t cpu_tlb_invalid_global_count;
		};
	};

	uint16_t                cpu_tlb_gen_counts_local[MAX_CPUS];
	uint16_t                cpu_tlb_gen_counts_global[MAX_CPUS];

	struct processor        *cpu_processor;
	struct real_descriptor  *cpu_ldtp;
	struct cpu_desc_table   *cpu_desc_tablep;
	cpu_desc_index_t        cpu_desc_index;
	int                     cpu_ldt;

#define HWINTCNT_SIZE 256
	uint32_t                cpu_hwIntCnt[HWINTCNT_SIZE];    /* Interrupt counts */
	uint64_t                cpu_hwIntpexits[HWINTCNT_SIZE];
	uint64_t                cpu_dr7; /* debug control register */
	uint64_t                cpu_int_event_time;     /* intr entry/exit time */
	pal_rtc_nanotime_t      *cpu_nanotime;          /* Nanotime info */
#if CONFIG_CPU_COUNTERS
	/* double-buffered performance counter data */
	uint64_t                *cpu_kpc_buf[2];
	/* PMC shadow and reload value buffers */
	uint64_t                *cpu_kpc_shadow;
	uint64_t                *cpu_kpc_reload;
	struct mt_cpu cpu_monotonic;
#endif /* CONFIG_CPU_COUNTERS */
	uint32_t                cpu_pmap_pcid_enabled;
	pcid_t                  cpu_active_pcid;
	pcid_t                  cpu_last_pcid;
	pcid_t                  cpu_kernel_pcid;
	volatile pcid_ref_t     *cpu_pmap_pcid_coherentp;
	volatile pcid_ref_t     *cpu_pmap_pcid_coherentp_kernel;
	pcid_cdata_t            *cpu_pcid_data;
#ifdef  PCID_STATS
	uint64_t                cpu_pmap_pcid_flushes;
	uint64_t                cpu_pmap_pcid_preserves;
#endif
	uint64_t                cpu_aperf;
	uint64_t                cpu_mperf;
	uint64_t                cpu_c3res;
	uint64_t                cpu_c6res;
	uint64_t                cpu_c7res;
	uint64_t                cpu_itime_total;
	uint64_t                cpu_rtime_total;
	uint64_t                cpu_ixtime;
	uint64_t                cpu_idle_exits;
	/*
	 * Note that the cacheline-copy mechanism uses the cpu_rtimes field in the shadow CPU
	 * structures to temporarily stash the code cacheline that includes the instruction
	 * pointer at the time of the fault (this field is otherwise unused in the shadow
	 * CPU structures).
	 */
	uint64_t                cpu_rtimes[CPU_RTIME_BINS];
	uint64_t                cpu_itimes[CPU_ITIME_BINS];
#if !CONFIG_CPU_COUNTERS
	uint64_t                cpu_cur_insns;
	uint64_t                cpu_cur_ucc;
	uint64_t                cpu_cur_urc;
#endif /* !CONFIG_CPU_COUNTERS */
	uint64_t                cpu_gpmcs[4];
	uint64_t                cpu_max_observed_int_latency;
	int                     cpu_max_observed_int_latency_vector;
	volatile boolean_t      cpu_NMI_acknowledged;
	uint64_t                debugger_entry_time;
	uint64_t                debugger_ipi_time;
	/* A separate nested interrupt stack flag, to account
	 * for non-nested interrupts arriving while on the interrupt stack
	 * Currently only occurs when AICPM enables interrupts on the
	 * interrupt stack during processor offlining.
	 */
	uint32_t                cpu_nested_istack;
	uint32_t                cpu_nested_istack_events;
	x86_saved_state64_t     *cpu_fatal_trap_state;
	x86_saved_state64_t     *cpu_post_fatal_trap_state;
#if CONFIG_VMX
	vmx_cpu_t               cpu_vmx;                /* wonderful world of virtualization */
#endif
#if CONFIG_MCA
	struct mca_state        *cpu_mca_state;         /* State at MC fault */
#endif
	int                     cpu_type;
	int                     cpu_subtype;
	int                     cpu_threadtype;
	boolean_t               cpu_iflag;
	boolean_t               cpu_boot_complete;
	int                     cpu_hibernate;
#define MAX_PREEMPTION_RECORDS (8)
#if     DEVELOPMENT || DEBUG
	int                     cpu_plri;
	plrecord_t              plrecords[MAX_PREEMPTION_RECORDS];
#endif
	struct x86_lcpu         lcpu;
	int                     cpu_phys_number;        /* Physical CPU */
	cpu_id_t                cpu_id;                 /* Platform Expert */
#if DEBUG
	uint64_t                cpu_entry_cr3;
	uint64_t                cpu_exit_cr3;
	uint64_t                cpu_pcid_last_cr3;
#endif
	boolean_t               cpu_rendezvous_in_progress;
#if CST_DEMOTION_DEBUG
	/* Count of thread wakeups issued by this processor */
	uint64_t                cpu_wakeups_issued_total;
#endif
#if DEBUG || DEVELOPMENT
	uint64_t                tsc_sync_delta;
#endif
	uint32_t                cpu_soft_apic_lvt_timer;
#if CONFIG_KCOV
	kcov_cpu_data_t         cpu_kcov_data;
#endif
} cpu_data_t;

extern cpu_data_t *__single cpu_data_ptr[MAX_CPUS];

/*
 * __SEG_GS marks %gs-relative operations:
 *   https://clang.llvm.org/docs/LanguageExtensions.html#memory-references-to-specified-segments
 *   https://gcc.gnu.org/onlinedocs/gcc/Named-Address-Spaces.html#x86-Named-Address-Spaces
 */
#if defined(__SEG_GS)
// __seg_gs exists
#elif defined(__clang__)
#define __seg_gs __attribute__((address_space(256)))
#else
#error use a compiler that supports address spaces or __seg_gs
#endif

#define CPU_DATA()            ((cpu_data_t __seg_gs *)0UL)

/*
 * Everyone within the osfmk part of the kernel can use the fast
 * inline versions of these routines.  Everyone outside, must call
 * the real thing,
 */


/*
 * The "volatile" flavor of current_thread() is intended for use by
 * scheduler code which may need to update the thread pointer in the
 * course of a context switch.  Any call to current_thread() made
 * prior to the thread pointer update should be safe to optimize away
 * as it should be consistent with that thread's state to the extent
 * the compiler can reason about it.  Likewise, the context switch
 * path will eventually result in an arbitrary branch to the new
 * thread's pc, about which the compiler won't be able to reason.
 * Thus any compile-time optimization of current_thread() calls made
 * within the new thread should be safely encapsulated in its
 * register/stack state.  The volatile form therefore exists to cover
 * the window between the thread pointer update and the branch to
 * the new pc.
 */
static inline thread_t
get_active_thread_volatile(void)
{
	return CPU_DATA()->cpu_active_thread;
}

static inline __attribute__((const)) thread_t
get_active_thread(void)
{
	return CPU_DATA()->cpu_active_thread;
}

#define current_thread_fast()           get_active_thread()
#define current_thread_volatile()       get_active_thread_volatile()

#define cpu_mode_is64bit()              TRUE

static inline int
get_preemption_level(void)
{
	return CPU_DATA()->cpu_preemption_level;
}
static inline int
get_interrupt_level(void)
{
	return CPU_DATA()->cpu_interrupt_level;
}
static inline int
get_cpu_number(void)
{
	return CPU_DATA()->cpu_number;
}
static inline vm_offset_t
get_current_percpu_base(void)
{
	return CPU_DATA()->cpu_pcpu_base;
}
static inline int
get_cpu_phys_number(void)
{
	return CPU_DATA()->cpu_phys_number;
}

static inline cpu_data_t *
current_cpu_datap(void)
{
	return CPU_DATA()->cpu_this;
}

/*
 * Facility to diagnose preemption-level imbalances, which are otherwise
 * challenging to debug. On each operation that enables or disables preemption,
 * we record a backtrace into a per-CPU ring buffer, along with the current
 * preemption level and operation type. Thus, if an imbalance is observed,
 * one can examine these per-CPU records to determine which codepath failed
 * to re-enable preemption, enabled premption without a corresponding
 * disablement etc. The backtracer determines which stack is currently active,
 * and uses that to perform bounds checks on unterminated stacks.
 * To enable, sysctl -w machdep.pltrace=1 on DEVELOPMENT or DEBUG kernels (DRK '15)
 * The bounds check currently doesn't account for non-default thread stack sizes.
 */
#if DEVELOPMENT || DEBUG
static inline void
rbtrace_bt(uint64_t *__counted_by(maxframes)rets, int maxframes,
    cpu_data_t *cdata, uint64_t frameptr, bool use_cursp)
{
	extern uint32_t         low_intstack[];         /* bottom */
	extern uint32_t         low_eintstack[];        /* top */
	extern char             mp_slave_stack[];
	int                     btidx = 0;

	uint64_t kstackb, kstackt;

	/* Obtain the 'current' program counter, initial backtrace
	 * element. This will also indicate if we were unable to
	 * trace further up the stack for some reason
	 */
	if (use_cursp) {
		__asm__ volatile ("leaq 1f(%%rip), %%rax; mov %%rax, %0\n1:"
                     : "=m" (rets[btidx++])
                     :
                     : "rax");
	}

	thread_t __single cplthread = cdata->cpu_active_thread;
	if (cplthread) {
		uintptr_t csp;
		if (use_cursp == true) {
			__asm__ __volatile__ ("movq %%rsp, %0": "=r" (csp):);
		} else {
			csp = frameptr;
		}
		/* Determine which stack we're on to populate stack bounds.
		 * We don't need to trace across stack boundaries for this
		 * routine.
		 */
		kstackb = cdata->cpu_active_stack;
		kstackt = kstackb + KERNEL_STACK_SIZE;
		if (csp < kstackb || csp > kstackt) {
			kstackt = cdata->cpu_kernel_stack;
			kstackb = kstackt - KERNEL_STACK_SIZE;
			if (csp < kstackb || csp > kstackt) {
				kstackt = cdata->cpu_int_stack_top;
				kstackb = kstackt - INTSTACK_SIZE;
				if (csp < kstackb || csp > kstackt) {
					kstackt = (uintptr_t)&low_eintstack;
					kstackb = kstackt - INTSTACK_SIZE;
					if (csp < kstackb || csp > kstackt) {
						kstackb = (uintptr_t)&mp_slave_stack;
						kstackt = kstackb + PAGE_SIZE;
					} else {
						kstackb = 0;
						kstackt = 0;
					}
				}
			}
		}

		if (__probable(kstackb && kstackt)) {
			uint64_t *cfp = __unsafe_forge_single(uint64_t *, frameptr);
			int rbbtf;

			for (rbbtf = btidx; rbbtf < maxframes; rbbtf++) {
				uint64_t cur_retp;
				/*
				 * cfp == 0 is covered by the first comparison, and we're guaranteed
				 * that kstackb is non-zero from the containing if block.  The os_add_overflow is
				 * necessary because it's not uncommon for backtraces to terminate with bogus
				 * frame pointers.
				 */
				if (((uint64_t)cfp < kstackb) || os_add_overflow((uint64_t)cfp, sizeof(uint64_t), &cur_retp) || cur_retp >= kstackt) {
					rets[rbbtf] = 0;
					continue;
				}
				rets[rbbtf] = *(uint64_t *)cur_retp;
				cfp = __unsafe_forge_single(uint64_t *, *cfp);
			}
		}
	}
}

__attribute__((noinline))
static inline void
pltrace_internal(boolean_t enable)
{
	cpu_data_t *cdata = current_cpu_datap();
	int cpli = cdata->cpu_preemption_level;
	int cplrecord = cdata->cpu_plri;
	uint64_t *plbts;

	assert(cpli >= 0);

	cdata->plrecords[cplrecord].pltype = enable;
	cdata->plrecords[cplrecord].plevel = cpli;

	plbts = &cdata->plrecords[cplrecord].plbt[0];

	cplrecord++;

	if (cplrecord >= MAX_PREEMPTION_RECORDS) {
		cplrecord = 0;
	}

	cdata->cpu_plri = cplrecord;

	rbtrace_bt(plbts, MAX_TRACE_BTFRAMES - 1, cdata, (uint64_t)__builtin_frame_address(0), false);
}

extern int plctrace_enabled;

static inline uint32_t
traptrace_start(int vecnum, uint64_t ipc, uint64_t sabs, uint64_t frameptr)
{
	cpu_data_t *cdata;
	uint32_t nextidx;
	traptrace_entry_t *cur_traptrace_ring;
	uint32_t *nextidxp;

	if (__improbable(traptrace_enabled == 0 || traptrace_entries_per_cpu == 0)) {
		return TRAPTRACE_INVALID_INDEX;
	}

	assert(ml_get_interrupts_enabled() == FALSE);
	cdata = current_cpu_datap();
	nextidxp = PERCPU_GET(traptrace_next);
	nextidx = *nextidxp;
	/* prevent nested interrupts from clobbering this record */
	*nextidxp = (((nextidx + 1) >= (unsigned int)traptrace_entries_per_cpu) ? 0 : (nextidx + 1));

	cur_traptrace_ring = __unsafe_forge_bidi_indexable(traptrace_entry_t *,
	    *PERCPU_GET(traptrace_ring), sizeof(traptrace_entry_t) * traptrace_entries_per_cpu);
	cur_traptrace_ring[nextidx].vector = vecnum;
	cur_traptrace_ring[nextidx].curthread = current_thread_fast();
	cur_traptrace_ring[nextidx].interrupted_pc = ipc;
	cur_traptrace_ring[nextidx].curpl = cdata->cpu_preemption_level;
	cur_traptrace_ring[nextidx].curil = cdata->cpu_interrupt_level;
	cur_traptrace_ring[nextidx].start_time_abs = sabs;
	cur_traptrace_ring[nextidx].duration = ~0ULL;

	rbtrace_bt(&cur_traptrace_ring[nextidx].backtrace[0],
	    MAX_TRACE_BTFRAMES - 1, cdata, frameptr, false);

	assert(nextidx <= 0xFFFF);

	/*
	 * encode the cpu number we're on because traptrace_end()
	 * might be called from a different CPU.
	 */
	return ((uint32_t)cdata->cpu_number << 16) | nextidx;
}

static inline void
traptrace_end(uint32_t index, uint64_t eabs)
{
	traptrace_entry_t *__unsafe_indexable ring;

	if (index != TRAPTRACE_INVALID_INDEX) {
		ring = *PERCPU_GET_WITH_BASE(other_percpu_base(index >> 16),
		    traptrace_ring);
		index &= 0XFFFF;
		ring[index].duration = eabs - ring[index].start_time_abs;
	}
}

#endif /* DEVELOPMENT || DEBUG */

__header_always_inline void
pltrace(boolean_t plenable)
{
#if DEVELOPMENT || DEBUG
	if (__improbable(plctrace_enabled != 0)) {
		pltrace_internal(plenable);
	}
#else
	(void)plenable;
#endif
}

static inline void
disable_preemption_internal(void)
{
	assert(get_preemption_level() >= 0);

	os_compiler_barrier();
	CPU_DATA()->cpu_preemption_level++;
	os_compiler_barrier();
	pltrace(FALSE);
}

static inline void
enable_preemption_internal(void)
{
	assert(get_preemption_level() > 0);
	pltrace(TRUE);
	os_compiler_barrier();
	if (0 == --CPU_DATA()->cpu_preemption_level) {
		kernel_preempt_check();
	}
	os_compiler_barrier();
}

static inline void
enable_preemption_no_check(void)
{
	assert(get_preemption_level() > 0);

	pltrace(TRUE);
	os_compiler_barrier();
	CPU_DATA()->cpu_preemption_level--;
	os_compiler_barrier();
}

static inline void
_enable_preemption_no_check(void)
{
	enable_preemption_no_check();
}

static inline void
mp_disable_preemption(void)
{
	disable_preemption_internal();
}

static inline void
_mp_disable_preemption(void)
{
	disable_preemption_internal();
}

static inline void
mp_enable_preemption(void)
{
	enable_preemption_internal();
}

static inline void
_mp_enable_preemption(void)
{
	enable_preemption_internal();
}

static inline void
mp_enable_preemption_no_check(void)
{
	enable_preemption_no_check();
}

static inline void
_mp_enable_preemption_no_check(void)
{
	enable_preemption_no_check();
}

#ifdef XNU_KERNEL_PRIVATE
#define disable_preemption() disable_preemption_internal()
#define disable_preemption_without_measurements() disable_preemption_internal()
#define enable_preemption() enable_preemption_internal()
#define MACHINE_PREEMPTION_MACROS (1)
#endif

static inline cpu_data_t *
cpu_datap(int cpu)
{
	return cpu_data_ptr[cpu];
}

static inline int
cpu_is_running(int cpu)
{
	return (cpu_datap(cpu) != NULL) && (cpu_datap(cpu)->cpu_running);
}

#ifdef MACH_KERNEL_PRIVATE
static inline cpu_data_t *
cpu_shadowp(int cpu)
{
	return cpu_data_ptr[cpu]->cd_shadow;
}

#endif
extern cpu_data_t *cpu_data_alloc(boolean_t is_boot_cpu);
extern void cpu_data_realloc(void);

#endif  /* I386_CPU_DATA */
