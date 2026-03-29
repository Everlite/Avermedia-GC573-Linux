savedcmd_utils/misc/g_queue.o := clang -Wp,-MMD,utils/misc/.g_queue.o.d -nostdinc -I/usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include -I/usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated -I/usr/lib/modules/6.19.10-1-cachyos/build/include -I/usr/lib/modules/6.19.10-1-cachyos/build/include -I/usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/uapi -I/usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/uapi -I/usr/lib/modules/6.19.10-1-cachyos/build/include/uapi -I/usr/lib/modules/6.19.10-1-cachyos/build/include/generated/uapi -include /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/compiler-version.h -include /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kconfig.h -include /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/compiler_types.h -D__KERNEL__ --target=x86_64-linux-gnu -fintegrated-as -Werror=ignored-optimization-argument -Werror=option-ignored -std=gnu11 -fshort-wchar -funsigned-char -fno-common -fno-PIE -fno-strict-aliasing -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -mno-sse4a -fcf-protection=branch -fno-jump-tables -m64 -falign-loops=1 -mno-80387 -mno-fp-ret-in-387 -mstack-alignment=8 -mskip-rax-setup -march=x86-64-v3 -mno-red-zone -mcmodel=kernel -mstack-protector-guard-reg=gs -mstack-protector-guard-symbol=__ref_stack_chk_guard -Wno-sign-compare -fno-asynchronous-unwind-tables -mretpoline-external-thunk -mindirect-branch-cs-prefix -mfunction-return=thunk-extern -mharden-sls=all -fpatchable-function-entry=16,16 -fno-delete-null-pointer-checks -O3 -fstack-protector-strong -ftrivial-auto-var-init=zero -fno-stack-clash-protection -pg -mfentry -DCC_USING_NOP_MCOUNT -DCC_USING_FENTRY -fno-lto -flto=thin -fsplit-lto-unit -fvisibility=hidden -falign-functions=16 -fstrict-flex-arrays=3 -fms-extensions -fno-strict-overflow -fno-stack-check -fno-builtin-wcslen -Wall -Wextra -Wundef -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Wno-format-security -Wno-trigraphs -Wno-frame-address -Wno-address-of-packed-member -Wmissing-declarations -Wmissing-prototypes -Wframe-larger-than=2048 -Wno-gnu -Wno-microsoft-anon-tag -Wno-format-overflow-non-kprintf -Wno-format-truncation-non-kprintf -Wno-default-const-init-unsafe -Wno-pointer-sign -Wcast-function-type -Wno-unterminated-string-initialization -Wimplicit-fallthrough -Werror=date-time -Wenum-conversion -Wunused -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-format-overflow -Wno-override-init -Wno-pointer-to-enum-cast -Wno-tautological-constant-out-of-range-compare -Wno-unaligned-access -Wno-enum-compare-conditional -Wno-missing-field-initializers -Wno-type-limits -Wno-shift-negative-value -Wno-enum-enum-conversion -Wno-sign-compare -Wno-unused-parameter -g -gdwarf-5 -fcf-protection=none -fno-stack-protector -I./. -I././include -I././include/uapi -I././utils/alsa -I././utils/alsa/include -I././utils/gpio -I././utils/gpio/include -I././utils/i2c -I././utils/i2c/include -I././utils/mem -I././utils/mem/include -I././utils/misc -I././utils/misc/include -I././utils/pci -I././utils/pci/include -I././utils/thread -I././utils/thread/include -I././utils/trace -I././utils/trace/include -I././utils/v4l2 -I././utils/v4l2/include -I././board/cx511h -Wno-maybe-uninitialized -Wno-implicit-fallthrough -Wno-implicit-enum-enum-cast -Wno-missing-prototypes -Wno-unused-variable  -fdebug-info-for-profiling -mllvm -enable-fs-discriminator=true -mllvm -improved-fs-discriminator=true  -DMODULE  -DKBUILD_BASENAME='"g_queue"' -DKBUILD_MODNAME='"cx511h"' -D__KBUILD_MODNAME=cx511h -c -o utils/misc/g_queue.o utils/misc/g_queue.c  

source_utils/misc/g_queue.o := utils/misc/g_queue.c

deps_utils/misc/g_queue.o := \
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
  g_queue.h \
  cxt_mgr.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kernel.h \
    $(wildcard include/config/PREEMPT_VOLUNTARY_BUILD) \
    $(wildcard include/config/PREEMPT_DYNAMIC) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_CALL) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_KEY) \
    $(wildcard include/config/PREEMPT_) \
    $(wildcard include/config/DEBUG_ATOMIC_SLEEP) \
    $(wildcard include/config/SMP) \
    $(wildcard include/config/MMU) \
    $(wildcard include/config/PROVE_LOCKING) \
    $(wildcard include/config/TRACING) \
    $(wildcard include/config/DYNAMIC_FTRACE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/stdarg.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/align.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/align.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/const.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/const.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/array_size.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/compiler.h \
    $(wildcard include/config/TRACE_BRANCH_PROFILING) \
    $(wildcard include/config/PROFILE_ALL_BRANCHES) \
    $(wildcard include/config/OBJTOOL) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/generated/asm/rwonce.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/rwonce.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kasan-checks.h \
    $(wildcard include/config/KASAN_GENERIC) \
    $(wildcard include/config/KASAN_SW_TAGS) \
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
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kcsan-checks.h \
    $(wildcard include/config/KCSAN_WEAK_MEMORY) \
    $(wildcard include/config/KCSAN_IGNORE_ATOMICS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/limits.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/limits.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/limits.h \
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
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/container_of.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/build_bug.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/bitops.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/bits.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/vdso/bits.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/bits.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/overflow.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/const.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/typecheck.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/kernel.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/uapi/linux/sysinfo.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/generic-non-atomic.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/barrier.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/alternative.h \
    $(wildcard include/config/CALL_THUNKS) \
    $(wildcard include/config/MITIGATION_ITS) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/objtool.h \
    $(wildcard include/config/FRAME_POINTER) \
    $(wildcard include/config/NOINSTR_VALIDATION) \
    $(wildcard include/config/MITIGATION_UNRET_ENTRY) \
    $(wildcard include/config/MITIGATION_SRSO) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/objtool_types.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/annotate.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/asm.h \
    $(wildcard include/config/KPROBES) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/asm-offsets.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/generated/asm-offsets.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/extable_fixup_types.h \
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
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/printk.h \
    $(wildcard include/config/MESSAGE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_QUIET) \
    $(wildcard include/config/EARLY_PRINTK) \
    $(wildcard include/config/PRINTK) \
    $(wildcard include/config/PRINTK_INDEX) \
    $(wildcard include/config/DYNAMIC_DEBUG) \
    $(wildcard include/config/DYNAMIC_DEBUG_CORE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/init.h \
    $(wildcard include/config/MEMORY_HOTPLUG) \
    $(wildcard include/config/HAVE_ARCH_PREL32_RELOCATIONS) \
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
    $(wildcard include/config/NR_CPUS) \
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
    $(wildcard include/config/LOCKDEP) \
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
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/args.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/jump_label.h \
    $(wildcard include/config/HAVE_JUMP_LABEL_HACK) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/nops.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/barrier.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/bitops.h \
    $(wildcard include/config/X86_CMOV) \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/rmwcc.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/sched.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/arch_hweight.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/cpufeatures.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/const_hweight.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/instrumented-atomic.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/instrumented.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kmsan-checks.h \
    $(wildcard include/config/KMSAN) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/instrumented-non-atomic.h \
    $(wildcard include/config/KCSAN_ASSUME_PLAIN_WRITES_ATOMIC) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/instrumented-lock.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/le.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/bitops/ext2-atomic-setbit.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/hex.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/kstrtox.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/log2.h \
    $(wildcard include/config/ARCH_HAS_ILOG2_U32) \
    $(wildcard include/config/ARCH_HAS_ILOG2_U64) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/math.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/arch/x86/include/asm/div64.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/asm-generic/div64.h \
    $(wildcard include/config/CC_OPTIMIZE_FOR_PERFORMANCE) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/minmax.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/sprintf.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/instruction_pointer.h \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/util_macros.h \
    $(wildcard include/config/FOO_SUSPEND) \
  /usr/lib/modules/6.19.10-1-cachyos/build/include/linux/wordpart.h \
  queue.h \
  typedef.h \
  sys.h \
  debug.h \

utils/misc/g_queue.o: $(deps_utils/misc/g_queue.o)

$(deps_utils/misc/g_queue.o):
