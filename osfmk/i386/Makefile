export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir

include $(MakeInc_cmd)
include $(MakeInc_def)

EXPORT_ONLY_FILES =	\
		    apic.h \
		    asm.h \
		    atomic.h \
		    bit_routines.h \
		    cpu_number.h \
		    cpu_capabilities.h	\
		    cpu_data.h \
		    cpu_topology.h \
		    cpuid.h \
		    eflags.h \
		    fpu.h \
		    x86_hypercall.h \
		    lapic.h \
		    lock.h \
		    locks.h \
		    machine_routines.h \
		    machine_cpu.h \
		    memory_types.h \
		    mtrr.h \
		    mp.h \
		    mp_desc.h \
		    mp_events.h \
		    pal_native.h \
		    pal_routines.h \
		    pal_hibernate.h \
		    panic_hooks.h \
		    pmCPU.h \
		    pmap.h \
		    proc_reg.h \
		    rtclock_protos.h \
		    seg.h \
		    simple_lock.h \
		    smp.h \
	            trap.h \
		    tsc.h \
		    tss.h \
		    ucode.h \
		    vmx.h

INSTALL_MD_DIR = i386

# These are covered by ../../bsd/machine/machine.modulemap.
INSTALL_MD_LIST = eflags.h user_ldt.h

# These are covered by ../../bsd/machine/machine_private.modulemap.
INSTALL_MD_LCL_LIST = cpu_capabilities.h

INSTALL_SF_MD_LCL_LIST = ${INSTALL_MD_LIST} ${INSTALL_MD_LCL_LIST}

INSTALL_KF_MD_LIST = asm.h cpuid.h eflags.h locks.h machine_routines.h proc_reg.h vmx.h trap.h

INSTALL_KF_MD_LCL_LIST = $(filter-out bit_routines.h cpu_data.h pal_i386.h, $(EXPORT_ONLY_FILES))

EXPORT_MD_LIST = ${EXPORT_ONLY_FILES}

EXPORT_MD_DIR = i386

include $(MakeInc_rule)
include $(MakeInc_dir)
