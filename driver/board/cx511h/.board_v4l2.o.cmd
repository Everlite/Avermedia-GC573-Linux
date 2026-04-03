savedcmd_board/cx511h/board_v4l2.o := clang -Wp,-MMD,board/cx511h/.board_v4l2.o.d -nostdinc -I/usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include -I/usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated -I/usr/lib/modules/6.19.10-1-cachyos/build/include -I/usr/lib/modules/6.19.10-1-cachyos/build/include -I/usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi -I/usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/uapi -I/usr/lib/modules/6.19.10-1-cachyos/build/include/uapi -I/usr/lib/modules/6.19.10-1-cachyos/build/include/generated/uapi -include /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/compiler-version.h -include /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kconfig.h -include /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/compiler_types.h -D__KERNEL__ --target=x86_64-linux-gnu -fintegrated-as -Werror=ignored-optimization-argument -Werror=option-ignored -std=gnu11 -fshort-wchar -funsigned-char -fno-common -fno-PIE -fno-strict-aliasing -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -mno-sse4a -fcf-protection=branch -fno-jump-tables -m64 -falign-loops=1 -mno-80387 -mno-fp-ret-in-387 -mstack-alignment=8 -mskip-rax-setup -march=x86-64-v3 -mno-red-zone -mcmodel=kernel -mstack-protector-guard-reg=gs -mstack-protector-guard-symbol=__ref_stack_chk_guard -Wno-sign-compare -fno-asynchronous-unwind-tables -mretpoline-external-thunk -mindirect-branch-cs-prefix -mfunction-return=thunk-extern -mharden-sls=all -fpatchable-function-entry=16,16 -fno-delete-null-pointer-checks -O3 -fstack-protector-strong -ftrivial-auto-var-init=zero -fno-stack-clash-protection -pg -mfentry -DCC_USING_NOP_MCOUNT -DCC_USING_FENTRY -fno-lto -flto=thin -fsplit-lto-unit -fvisibility=hidden -falign-functions=16 -fstrict-flex-arrays=3 -fms-extensions -fno-strict-overflow -fno-stack-check -fno-builtin-wcslen -Wall -Wextra -Wundef -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Wno-format-security -Wno-trigraphs -Wno-frame-address -Wno-address-of-packed-member -Wmissing-declarations -Wmissing-prototypes -Wframe-larger-than=2048 -Wno-gnu -Wno-microsoft-anon-tag -Wno-format-overflow-non-kprintf -Wno-format-truncation-non-kprintf -Wno-default-const-init-unsafe -Wno-pointer-sign -Wcast-function-type -Wno-unterminated-string-initialization -Wimplicit-fallthrough -Werror=date-time -Wenum-conversion -Wunused -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-format-overflow -Wno-override-init -Wno-pointer-to-enum-cast -Wno-tautological-constant-out-of-range-compare -Wno-unaligned-access -Wno-enum-compare-conditional -Wno-missing-field-initializers -Wno-type-limits -Wno-shift-negative-value -Wno-enum-enum-conversion -Wno-sign-compare -Wno-unused-parameter -g -gdwarf-5 -fcf-protection=none -fno-stack-protector -I./. -I././include -I././include/uapi -I././utils/alsa -I././utils/alsa/include -I././utils/gpio -I././utils/gpio/include -I././utils/i2c -I././utils/i2c/include -I././utils/mem -I././utils/mem/include -I././utils/misc -I././utils/misc/include -I././utils/pci -I././utils/pci/include -I././utils/thread -I././utils/thread/include -I././utils/trace -I././utils/trace/include -I././utils/v4l2 -I././utils/v4l2/include -I././board/cx511h -Wno-maybe-uninitialized -Wno-implicit-fallthrough -Wno-implicit-enum-enum-cast -Wno-missing-prototypes -Wno-unused-variable  -fdebug-info-for-profiling -mllvm -enable-fs-discriminator=true -mllvm -improved-fs-discriminator=true  -DMODULE  -DKBUILD_BASENAME='"board_v4l2"' -DKBUILD_MODNAME='"cx511h"' -D__KBUILD_MODNAME=cx511h -c -o board/cx511h/board_v4l2.o board/cx511h/board_v4l2.c  

source_board/cx511h/board_v4l2.o := board/cx511h/board_v4l2.c

deps_board/cx511h/board_v4l2.o := \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/compiler-version.h \
    $(wildcard include/config/CC_VERSION_TEXT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kconfig.h \
    $(wildcard include/config/CPU_BIG_ENDIAN) \
    $(wildcard include/config/BOOGER) \
    $(wildcard include/config/FOO) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/compiler_types.h \
    $(wildcard include/config/DEBUG_INFO_BTF) \
    $(wildcard include/config/PAHOLE_HAS_BTF_TAG) \
    $(wildcard include/config/FUNCTION_ALIGNMENT) \
    $(wildcard include/config/CC_HAS_SANE_FUNCTION_ALIGNMENT) \
    $(wildcard include/config/X86_64) \
    $(wildcard include/config/ARM64) \
    $(wildcard include/config/LD_DEAD_CODE_DATA_ELIMINATION) \
    $(wildcard include/config/LTO_CLANG) \
    $(wildcard include/config/HAVE_ARCH_COMPILER_H) \
    $(wildcard include/config/KCSAN) \
    $(wildcard include/config/CC_HAS_ASSUME) \
    $(wildcard include/config/CC_HAS_COUNTED_BY) \
    $(wildcard include/config/CC_HAS_MULTIDIMENSIONAL_NONSTRING) \
    $(wildcard include/config/UBSAN_INTEGER_WRAP) \
    $(wildcard include/config/CFI) \
    $(wildcard include/config/ARCH_USES_CFI_GENERIC_LLVM_PASS) \
    $(wildcard include/config/CC_HAS_ASM_INLINE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/compiler_attributes.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/compiler-clang.h \
    $(wildcard include/config/ARCH_USE_BUILTIN_BSWAP) \
    $(wildcard include/config/CC_HAS_TYPEOF_UNQUAL) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/delay.h \
    $(wildcard include/config/HIGH_RES_TIMERS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/math.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/types.h \
    $(wildcard include/config/HAVE_UID16) \
    $(wildcard include/config/UID16) \
    $(wildcard include/config/ARCH_DMA_ADDR_T_64BIT) \
    $(wildcard include/config/PHYS_ADDR_T_64BIT) \
    $(wildcard include/config/64BIT) \
    $(wildcard include/config/ARCH_32BIT_USTAT_F_TINODE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/uapi/asm/types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/asm-generic/types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/int-ll64.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/asm-generic/int-ll64.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/bitsperlong.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitsperlong.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/asm-generic/bitsperlong.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/posix_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/stddef.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/stddef.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/posix_types.h \
    $(wildcard include/config/X86_32) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/posix_types_64.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/asm-generic/posix_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/div64.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/div64.h \
    $(wildcard include/config/CC_OPTIMIZE_FOR_PERFORMANCE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/compiler.h \
    $(wildcard include/config/TRACE_BRANCH_PROFILING) \
    $(wildcard include/config/PROFILE_ALL_BRANCHES) \
    $(wildcard include/config/OBJTOOL) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/asm/rwonce.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/rwonce.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kasan-checks.h \
    $(wildcard include/config/KASAN_GENERIC) \
    $(wildcard include/config/KASAN_SW_TAGS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kcsan-checks.h \
    $(wildcard include/config/KCSAN_WEAK_MEMORY) \
    $(wildcard include/config/KCSAN_IGNORE_ATOMICS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/kernel.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/sysinfo.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/const.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/const.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/const.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/sched.h \
    $(wildcard include/config/LOCKDEP) \
    $(wildcard include/config/DEBUG_ATOMIC_SLEEP) \
    $(wildcard include/config/PREEMPT_RT) \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_NATIVE) \
    $(wildcard include/config/SCHED_INFO) \
    $(wildcard include/config/SCHEDSTATS) \
    $(wildcard include/config/SCHED_CORE) \
    $(wildcard include/config/FAIR_GROUP_SCHED) \
    $(wildcard include/config/RT_GROUP_SCHED) \
    $(wildcard include/config/RT_MUTEXES) \
    $(wildcard include/config/UCLAMP_TASK) \
    $(wildcard include/config/UCLAMP_BUCKETS_COUNT) \
    $(wildcard include/config/KMAP_LOCAL) \
    $(wildcard include/config/THREAD_INFO_IN_TASK) \
    $(wildcard include/config/MEM_ALLOC_PROFILING) \
    $(wildcard include/config/SCHED_CLASS_EXT) \
    $(wildcard include/config/CGROUP_SCHED) \
    $(wildcard include/config/CFS_BANDWIDTH) \
    $(wildcard include/config/PREEMPT_NOTIFIERS) \
    $(wildcard include/config/BLK_DEV_IO_TRACE) \
    $(wildcard include/config/PREEMPT_RCU) \
    $(wildcard include/config/TASKS_RCU) \
    $(wildcard include/config/TASKS_TRACE_RCU) \
    $(wildcard include/config/MEMCG_V1) \
    $(wildcard include/config/LRU_GEN) \
    $(wildcard include/config/COMPAT_BRK) \
    $(wildcard include/config/CGROUPS) \
    $(wildcard include/config/BLK_CGROUP) \
    $(wildcard include/config/PSI) \
    $(wildcard include/config/PAGE_OWNER) \
    $(wildcard include/config/EVENTFD) \
    $(wildcard include/config/ARCH_HAS_CPU_PASID) \
    $(wildcard include/config/X86_BUS_LOCK_DETECT) \
    $(wildcard include/config/TASK_DELAY_ACCT) \
    $(wildcard include/config/STACKPROTECTOR) \
    $(wildcard include/config/ARCH_HAS_SCALED_CPUTIME) \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_GEN) \
    $(wildcard include/config/NO_HZ_FULL) \
    $(wildcard include/config/POSIX_CPUTIMERS) \
    $(wildcard include/config/POSIX_CPU_TIMERS_TASK_WORK) \
    $(wildcard include/config/KEYS) \
    $(wildcard include/config/SYSVIPC) \
    $(wildcard include/config/DETECT_HUNG_TASK) \
    $(wildcard include/config/IO_URING) \
    $(wildcard include/config/AUDIT) \
    $(wildcard include/config/AUDITSYSCALL) \
    $(wildcard include/config/DETECT_HUNG_TASK_BLOCKER) \
    $(wildcard include/config/TRACE_IRQFLAGS) \
    $(wildcard include/config/UBSAN) \
    $(wildcard include/config/UBSAN_TRAP) \
    $(wildcard include/config/COMPACTION) \
    $(wildcard include/config/TASK_XACCT) \
    $(wildcard include/config/CPUSETS) \
    $(wildcard include/config/X86_CPU_RESCTRL) \
    $(wildcard include/config/FUTEX) \
    $(wildcard include/config/COMPAT) \
    $(wildcard include/config/PERF_EVENTS) \
    $(wildcard include/config/DEBUG_PREEMPT) \
    $(wildcard include/config/NUMA) \
    $(wildcard include/config/NUMA_BALANCING) \
    $(wildcard include/config/FAULT_INJECTION) \
    $(wildcard include/config/LATENCYTOP) \
    $(wildcard include/config/KMSAN) \
    $(wildcard include/config/KUNIT) \
    $(wildcard include/config/FUNCTION_GRAPH_TRACER) \
    $(wildcard include/config/TRACING) \
    $(wildcard include/config/KCOV) \
    $(wildcard include/config/MEMCG) \
    $(wildcard include/config/UPROBES) \
    $(wildcard include/config/BCACHE) \
    $(wildcard include/config/MMU) \
    $(wildcard include/config/VMAP_STACK) \
    $(wildcard include/config/LIVEPATCH) \
    $(wildcard include/config/SECURITY) \
    $(wildcard include/config/BPF_SYSCALL) \
    $(wildcard include/config/KSTACK_ERASE) \
    $(wildcard include/config/KSTACK_ERASE_METRICS) \
    $(wildcard include/config/X86_MCE) \
    $(wildcard include/config/KRETPROBES) \
    $(wildcard include/config/RETHOOK) \
    $(wildcard include/config/ARCH_HAS_PARANOID_L1D_FLUSH) \
    $(wildcard include/config/RV) \
    $(wildcard include/config/RV_PER_TASK_MONITORS) \
    $(wildcard include/config/USER_EVENTS) \
    $(wildcard include/config/UNWIND_USER) \
    $(wildcard include/config/SCHED_PROXY_EXEC) \
    $(wildcard include/config/PREEMPTION) \
    $(wildcard include/config/PREEMPT_DYNAMIC) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_CALL) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_KEY) \
    $(wildcard include/config/SMP) \
    $(wildcard include/config/MEM_ALLOC_PROFILING_DEBUG) \
    $(wildcard include/config/SCHED_MM_CID) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/sched.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/current.h \
    $(wildcard include/config/USE_X86_SEG_SUPPORT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/build_bug.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/cache.h \
    $(wildcard include/config/ARCH_HAS_CACHE_LINE_SIZE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/cache.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/cache.h \
    $(wildcard include/config/X86_L1_CACHE_SHIFT) \
    $(wildcard include/config/X86_INTERNODE_CACHE_SHIFT) \
    $(wildcard include/config/X86_VSMP) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/linkage.h \
    $(wildcard include/config/ARCH_USE_SYM_ANNOTATIONS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/stringify.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/export.h \
    $(wildcard include/config/MODVERSIONS) \
    $(wildcard include/config/GENDWARFKSYMS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/linkage.h \
    $(wildcard include/config/CALL_PADDING) \
    $(wildcard include/config/MITIGATION_RETHUNK) \
    $(wildcard include/config/MITIGATION_RETPOLINE) \
    $(wildcard include/config/MITIGATION_SLS) \
    $(wildcard include/config/FUNCTION_PADDING_BYTES) \
    $(wildcard include/config/UML) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/ibt.h \
    $(wildcard include/config/X86_KERNEL_IBT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/percpu.h \
    $(wildcard include/config/CC_HAS_NAMED_AS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/args.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/bits.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/bits.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/bits.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/overflow.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/limits.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/limits.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/limits.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/asm.h \
    $(wildcard include/config/KPROBES) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/annotate.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/objtool_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/asm-offsets.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/generated/asm-offsets.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/extable_fixup_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/percpu.h \
    $(wildcard include/config/HAVE_SETUP_PER_CPU_AREA) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/threads.h \
    $(wildcard include/config/NR_CPUS) \
    $(wildcard include/config/BASE_SMALL) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/percpu-defs.h \
    $(wildcard include/config/ARCH_MODULE_NEEDS_WEAK_PER_CPU) \
    $(wildcard include/config/DEBUG_FORCE_WEAK_PER_CPU) \
    $(wildcard include/config/AMD_MEM_ENCRYPT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/processor.h \
    $(wildcard include/config/X86_VMX_FEATURE_NAMES) \
    $(wildcard include/config/X86_IOPL_IOPERM) \
    $(wildcard include/config/VM86) \
    $(wildcard include/config/X86_USER_SHADOW_STACK) \
    $(wildcard include/config/X86_DEBUG_FPU) \
    $(wildcard include/config/PARAVIRT_XXL) \
    $(wildcard include/config/CPU_SUP_AMD) \
    $(wildcard include/config/XEN) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/processor-flags.h \
    $(wildcard include/config/MITIGATION_PAGE_TABLE_ISOLATION) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/processor-flags.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/mem_encrypt.h \
    $(wildcard include/config/ARCH_HAS_MEM_ENCRYPT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/mem_encrypt.h \
    $(wildcard include/config/X86_MEM_ENCRYPT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/init.h \
    $(wildcard include/config/MEMORY_HOTPLUG) \
    $(wildcard include/config/HAVE_ARCH_PREL32_RELOCATIONS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/cc_platform.h \
    $(wildcard include/config/ARCH_HAS_CC_PLATFORM) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/math_emu.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/ptrace.h \
    $(wildcard include/config/PARAVIRT) \
    $(wildcard include/config/IA32_EMULATION) \
    $(wildcard include/config/X86_DEBUGCTLMSR) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/segment.h \
    $(wildcard include/config/XEN_PV) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/alternative.h \
    $(wildcard include/config/CALL_THUNKS) \
    $(wildcard include/config/MITIGATION_ITS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/objtool.h \
    $(wildcard include/config/FRAME_POINTER) \
    $(wildcard include/config/NOINSTR_VALIDATION) \
    $(wildcard include/config/MITIGATION_UNRET_ENTRY) \
    $(wildcard include/config/MITIGATION_SRSO) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/bug.h \
    $(wildcard include/config/GENERIC_BUG) \
    $(wildcard include/config/DEBUG_BUGVERBOSE) \
    $(wildcard include/config/DEBUG_BUGVERBOSE_DETAILED) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/instrumentation.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/static_call_types.h \
    $(wildcard include/config/HAVE_STATIC_CALL) \
    $(wildcard include/config/HAVE_STATIC_CALL_INLINE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bug.h \
    $(wildcard include/config/BUG) \
    $(wildcard include/config/GENERIC_BUG_RELATIVE_POINTERS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/once_lite.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/panic.h \
    $(wildcard include/config/PANIC_TIMEOUT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/stdarg.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/printk.h \
    $(wildcard include/config/MESSAGE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_QUIET) \
    $(wildcard include/config/EARLY_PRINTK) \
    $(wildcard include/config/PRINTK) \
    $(wildcard include/config/PRINTK_INDEX) \
    $(wildcard include/config/DYNAMIC_DEBUG) \
    $(wildcard include/config/DYNAMIC_DEBUG_CORE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kern_levels.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/ratelimit_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/param.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/uapi/asm/param.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/param.h \
    $(wildcard include/config/HZ) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/asm-generic/param.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/spinlock_types_raw.h \
    $(wildcard include/config/DEBUG_SPINLOCK) \
    $(wildcard include/config/DEBUG_LOCK_ALLOC) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/spinlock_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/qspinlock_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/qrwlock_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/byteorder.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/byteorder/little_endian.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/byteorder/little_endian.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/swab.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/swab.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/swab.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/byteorder/generic.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/lockdep_types.h \
    $(wildcard include/config/PROVE_RAW_LOCK_NESTING) \
    $(wildcard include/config/LOCK_STAT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/dynamic_debug.h \
    $(wildcard include/config/JUMP_LABEL) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/jump_label.h \
    $(wildcard include/config/HAVE_ARCH_JUMP_LABEL_RELATIVE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/cleanup.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/err.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/uapi/asm/errno.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/asm-generic/errno.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/asm-generic/errno-base.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/jump_label.h \
    $(wildcard include/config/HAVE_JUMP_LABEL_HACK) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/nops.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/page_types.h \
    $(wildcard include/config/PHYSICAL_START) \
    $(wildcard include/config/PHYSICAL_ALIGN) \
    $(wildcard include/config/DYNAMIC_PHYSICAL_MASK) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/page.h \
    $(wildcard include/config/PAGE_SHIFT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/page_64_types.h \
    $(wildcard include/config/KASAN) \
    $(wildcard include/config/RANDOMIZE_BASE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/kaslr.h \
    $(wildcard include/config/RANDOMIZE_MEMORY) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/ptrace.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/ptrace-abi.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/paravirt_types.h \
    $(wildcard include/config/ZERO_CALL_USED_REGS) \
    $(wildcard include/config/PARAVIRT_DEBUG) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/desc_defs.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/pgtable_types.h \
    $(wildcard include/config/X86_INTEL_MEMORY_PROTECTION_KEYS) \
    $(wildcard include/config/X86_PAE) \
    $(wildcard include/config/MEM_SOFT_DIRTY) \
    $(wildcard include/config/HAVE_ARCH_USERFAULTFD_WP) \
    $(wildcard include/config/PGTABLE_LEVELS) \
    $(wildcard include/config/PROC_FS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/pgtable_64_types.h \
    $(wildcard include/config/DEBUG_KMAP_LOCAL_FORCE_MAP) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/sparsemem.h \
    $(wildcard include/config/SPARSEMEM) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/nospec-branch.h \
    $(wildcard include/config/CALL_THUNKS_DEBUG) \
    $(wildcard include/config/MITIGATION_CALL_DEPTH_TRACKING) \
    $(wildcard include/config/MITIGATION_IBPB_ENTRY) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/static_key.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/cpufeatures.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/msr-index.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/unwind_hints.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/orc_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/GEN-for-each-reg.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/proto.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/ldt.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/sigcontext.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/cpuid/api.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/cpuid/types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/string.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/string_64.h \
    $(wildcard include/config/ARCH_HAS_UACCESS_FLUSHCACHE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/paravirt.h \
    $(wildcard include/config/PARAVIRT_SPINLOCKS) \
    $(wildcard include/config/DEBUG_ENTRY) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/bug.h \
    $(wildcard include/config/BUG_ON_DATA_CORRUPTION) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/cpumask.h \
    $(wildcard include/config/FORCE_NR_CPUS) \
    $(wildcard include/config/HOTPLUG_CPU) \
    $(wildcard include/config/DEBUG_PER_CPU_MAPS) \
    $(wildcard include/config/CPUMASK_OFFSTACK) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/atomic.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/atomic.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/cmpxchg.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/cmpxchg_64.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/rmwcc.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/barrier.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/barrier.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/atomic64_64.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/atomic/atomic-arch-fallback.h \
    $(wildcard include/config/GENERIC_ATOMIC64) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/atomic/atomic-long.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/atomic/atomic-instrumented.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/instrumented.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kmsan-checks.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/bitmap.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/align.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/align.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/bitops.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/typecheck.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/generic-non-atomic.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/bitops.h \
    $(wildcard include/config/X86_CMOV) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/sched.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/arch_hweight.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/const_hweight.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/instrumented-atomic.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/instrumented-non-atomic.h \
    $(wildcard include/config/KCSAN_ASSUME_PLAIN_WRITES_ATOMIC) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/instrumented-lock.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/le.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/ext2-atomic-setbit.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/errno.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/errno.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/find.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/string.h \
    $(wildcard include/config/BINARY_PRINTF) \
    $(wildcard include/config/FORTIFY_SOURCE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/array_size.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/string.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/fortify-string.h \
    $(wildcard include/config/CC_HAS_KASAN_MEMINTRINSIC_PREFIX) \
    $(wildcard include/config/GENERIC_ENTRY) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/bitfield.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/bitmap-str.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/cpumask_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/gfp_types.h \
    $(wildcard include/config/KASAN_HW_TAGS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/numa.h \
    $(wildcard include/config/NUMA_KEEP_MEMINFO) \
    $(wildcard include/config/HAVE_ARCH_NODE_DEV_GROUP) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/nodemask.h \
    $(wildcard include/config/HIGHMEM) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/minmax.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/nodemask_types.h \
    $(wildcard include/config/NODES_SHIFT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/random.h \
    $(wildcard include/config/VMGENID) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kernel.h \
    $(wildcard include/config/PREEMPT_VOLUNTARY_BUILD) \
    $(wildcard include/config/PREEMPT_) \
    $(wildcard include/config/PROVE_LOCKING) \
    $(wildcard include/config/DYNAMIC_FTRACE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/container_of.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/hex.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kstrtox.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/log2.h \
    $(wildcard include/config/ARCH_HAS_ILOG2_U32) \
    $(wildcard include/config/ARCH_HAS_ILOG2_U64) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/sprintf.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/instruction_pointer.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/util_macros.h \
    $(wildcard include/config/FOO_SUSPEND) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/wordpart.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/list.h \
    $(wildcard include/config/LIST_HARDENED) \
    $(wildcard include/config/DEBUG_LIST) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/poison.h \
    $(wildcard include/config/ILLEGAL_POINTER_VALUE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/random.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/ioctl.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/uapi/asm/ioctl.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/ioctl.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/asm-generic/ioctl.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/irqnr.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/irqnr.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/frame.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/page.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/page_64.h \
    $(wildcard include/config/DEBUG_VIRTUAL) \
    $(wildcard include/config/X86_VSYSCALL_EMULATION) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/mmdebug.h \
    $(wildcard include/config/DEBUG_VM) \
    $(wildcard include/config/DEBUG_VM_IRQSOFF) \
    $(wildcard include/config/DEBUG_VM_PGFLAGS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/range.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/memory_model.h \
    $(wildcard include/config/FLATMEM) \
    $(wildcard include/config/SPARSEMEM_VMEMMAP) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/pfn.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/getorder.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/special_insns.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/irqflags.h \
    $(wildcard include/config/IRQSOFF_TRACER) \
    $(wildcard include/config/PREEMPT_TRACER) \
    $(wildcard include/config/DEBUG_IRQFLAGS) \
    $(wildcard include/config/TRACE_IRQFLAGS_SUPPORT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/irqflags_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/irqflags.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/fpu/types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/vmxfeatures.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/vdso/processor.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/shstk.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/personality.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/personality.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/math64.h \
    $(wildcard include/config/ARCH_SUPPORTS_INT128) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/math64.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/thread_info.h \
    $(wildcard include/config/ARCH_HAS_PREEMPT_LAZY) \
    $(wildcard include/config/HAVE_ARCH_WITHIN_STACK_FRAMES) \
    $(wildcard include/config/SH) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/restart_block.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/thread_info.h \
    $(wildcard include/config/X86_FRED) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/cpufeature.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/asm/cpufeaturemasks.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/thread_info_tif.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/preempt.h \
    $(wildcard include/config/PREEMPT_COUNT) \
    $(wildcard include/config/TRACE_PREEMPT_TOGGLE) \
    $(wildcard include/config/PREEMPT_NONE) \
    $(wildcard include/config/PREEMPT_VOLUNTARY) \
    $(wildcard include/config/PREEMPT) \
    $(wildcard include/config/PREEMPT_LAZY) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/preempt.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/smp_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/llist.h \
    $(wildcard include/config/ARCH_HAVE_NMI_SAFE_CMPXCHG) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/pid_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/sem_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/shm.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/shmparam.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kmsan_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/mutex_types.h \
    $(wildcard include/config/MUTEX_SPIN_ON_OWNER) \
    $(wildcard include/config/DEBUG_MUTEXES) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/osq_lock.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/spinlock_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/rwlock_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/plist_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/hrtimer_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/timerqueue_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/rbtree_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/timer_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/seccomp_types.h \
    $(wildcard include/config/SECCOMP) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/refcount_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/resource.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/resource.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/time_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/uapi/asm/resource.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/resource.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/asm-generic/resource.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/latencytop.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/sched/prio.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/sched/types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/signal_types.h \
    $(wildcard include/config/OLD_SIGACTION) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/signal.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/signal.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/signal.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/asm-generic/signal-defs.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/siginfo.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/asm-generic/siginfo.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/spinlock.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/bottom_half.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/lockdep.h \
    $(wildcard include/config/DEBUG_LOCKING_API_SELFTESTS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/smp.h \
    $(wildcard include/config/UP_LATE_INIT) \
    $(wildcard include/config/CSD_LOCK_WAIT_DEBUG) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/smp.h \
    $(wildcard include/config/DEBUG_NMI_SELFTEST) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/cpumask.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/asm/mmiowb.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/mmiowb.h \
    $(wildcard include/config/MMIOWB) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/spinlock.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/qspinlock.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/qspinlock.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/qrwlock.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/qrwlock.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/rwlock.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/spinlock_api_smp.h \
    $(wildcard include/config/INLINE_SPIN_LOCK) \
    $(wildcard include/config/INLINE_SPIN_LOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK_BH) \
    $(wildcard include/config/UNINLINE_SPIN_UNLOCK) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/GENERIC_LOCKBREAK) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/rwlock_api_smp.h \
    $(wildcard include/config/INLINE_READ_LOCK) \
    $(wildcard include/config/INLINE_WRITE_LOCK) \
    $(wildcard include/config/INLINE_READ_LOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_LOCK_BH) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_READ_TRYLOCK) \
    $(wildcard include/config/INLINE_WRITE_TRYLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_BH) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQRESTORE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/syscall_user_dispatch_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/mm_types_task.h \
    $(wildcard include/config/ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/tlbbatch.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/netdevice_xmit.h \
    $(wildcard include/config/NET_ACT_MIRRED) \
    $(wildcard include/config/NET_EGRESS) \
    $(wildcard include/config/NF_DUP_NETDEV) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/task_io_accounting.h \
    $(wildcard include/config/TASK_IO_ACCOUNTING) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/posix-timers_types.h \
    $(wildcard include/config/POSIX_TIMERS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/rseq_types.h \
    $(wildcard include/config/RSEQ) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/irq_work_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/workqueue_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/seqlock_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kcsan.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/rv.h \
    $(wildcard include/config/RV_LTL_MONITOR) \
    $(wildcard include/config/RV_REACTORS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/uidgid_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/tracepoint-defs.h \
    $(wildcard include/config/TRACEPOINTS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/unwind_deferred_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/asm/kmap_size.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/kmap_size.h \
    $(wildcard include/config/DEBUG_KMAP_LOCAL) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/generated/rq-offsets.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/sched/ext.h \
    $(wildcard include/config/EXT_GROUP_SCHED) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/rhashtable-types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/alloc_tag.h \
    $(wildcard include/config/MEM_ALLOC_PROFILING_ENABLED_BY_DEFAULT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/codetag.h \
    $(wildcard include/config/MODULES) \
    $(wildcard include/config/CODE_TAGGING) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/mutex.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/debug_locks.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/jiffies.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/time.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/time64.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/time64.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/time.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/time32.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/timex.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/timex.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/timex.h \
    $(wildcard include/config/X86_TSC) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/tsc.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/msr.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi/asm/msr.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/shared/msr.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/percpu.h \
    $(wildcard include/config/RANDOM_KMALLOC_CACHES) \
    $(wildcard include/config/PAGE_SIZE_4KB) \
    $(wildcard include/config/NEED_PER_CPU_PAGE_FIRST_CHUNK) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/time32.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/time.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/jiffies.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/generated/timeconst.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/delay.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/delay.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/moduleparam.h \
    $(wildcard include/config/ALPHA) \
    $(wildcard include/config/PPC64) \
    $(wildcard include/config/SYSFS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/io.h \
    $(wildcard include/config/HAS_IOPORT_MAP) \
    $(wildcard include/config/PCI) \
    $(wildcard include/config/STRICT_DEVMEM) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/sizes.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/io.h \
    $(wildcard include/config/MTRR) \
    $(wildcard include/config/X86_PAT) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/asm/early_ioremap.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/early_ioremap.h \
    $(wildcard include/config/GENERIC_EARLY_IOREMAP) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/shared/io.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/io.h \
    $(wildcard include/config/GENERIC_IOMAP) \
    $(wildcard include/config/TRACE_MMIO_ACCESS) \
    $(wildcard include/config/HAS_IOPORT) \
    $(wildcard include/config/GENERIC_IOREMAP) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/iomap.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/pci_iomap.h \
    $(wildcard include/config/NO_GENERIC_PCI_IOPORT_MAP) \
    $(wildcard include/config/GENERIC_PCI_IOMAP) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/logic_pio.h \
    $(wildcard include/config/INDIRECT_PIO) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/fwnode.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/videodev2.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/videodev2.h \
    $(wildcard include/config/VIDEO_ADV_DEBUG) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/v4l2-common.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/v4l2-controls.h \
  board.h \
  cxt_mgr.h \
  queue.h \
  typedef.h \
  sys.h \
  framegrabber.h \
  mem_model.h \
  v4l2_model.h \
  board/cx511h/board_v4l2.h \
  debug.h \
  pci_model.h \
  i2c_model.h \
  task_model.h \
  aver_xilinx.h \
  ite6805.h \
  trace.h \
  ite6664.h \
  version.h \
  it6664_typedef.h \
  it6664_extern.h \
  it6664_IO.h \
  it6664_config.h \
  it6664_EDID.h \
  it6664_hdmi2_rx.h \
  ite6805_dev_define.h \
  ite6805_config.h \
  board/cx511h/board_alsa.h \
  board/cx511h/board_i2c.h \

board/cx511h/board_v4l2.o: $(deps_board/cx511h/board_v4l2.o)

$(deps_board/cx511h/board_v4l2.o):
