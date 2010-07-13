#ifndef _I386_ALTERNATIVE_H
#define _I386_ALTERNATIVE_H

#include <asm/types.h>
#include <linux/stddef.h>
#include <linux/types.h>

struct alt_instr {
	u8 *instr; 		/* original instruction */
	u8 *replacement;
	u8  cpuid;		/* cpuid bit set for replacement */
	u8  instrlen;		/* length of original instruction */
	u8  replacementlen; 	/* length of new instruction, <= instrlen */
	u8  pad;
};

extern void alternative_instructions(void);
extern void apply_alternatives(struct alt_instr *start, struct alt_instr *end);

struct module;
#ifdef CONFIG_SMP
extern void alternatives_smp_module_add(struct module *mod, char *name,
					void *locks, void *locks_end,
					void *text, void *text_end);
extern void alternatives_smp_module_del(struct module *mod);
extern void alternatives_smp_switch(int smp);
#else
static inline void alternatives_smp_module_add(struct module *mod, char *name,
					void *locks, void *locks_end,
					void *text, void *text_end) {}
static inline void alternatives_smp_module_del(struct module *mod) {}
static inline void alternatives_smp_switch(int smp) {}
#endif	/* CONFIG_SMP */

/*
 * Alternative instructions for different CPU types or capabilities.
 *
 * This allows to use optimized instructions even on generic binary
 * kernels.
 *
 * length of oldinstr must be longer or equal the length of newinstr
 * It can be padded with nops as needed.
 *
 * For non barrier like inlines please define new variants
 * without volatile and memory clobber.
 */
#ifndef __WINKVM__
#define alternative(oldinstr, newinstr, feature)			\
	asm volatile ("661:\n\t" oldinstr "\n662:\n" 			\
		      ".section .altinstructions,\"a\"\n"		\
		      "  .align 4\n"					\
		      "  .long 661b\n"            /* label */		\
		      "  .long 663f\n"		  /* new instruction */	\
		      "  .byte %c0\n"             /* feature bit */	\
		      "  .byte 662b-661b\n"       /* sourcelen */	\
		      "  .byte 664f-663f\n"       /* replacementlen */	\
		      ".previous\n"					\
		      ".section .altinstr_replacement,\"ax\"\n"		\
		      "663:\n\t" newinstr "\n664:\n"   /* replacement */\
		      ".previous" :: "i" (feature) : "memory")
#else
#define alternative(oldinstr, newinstr, feature)			\
	asm volatile ("661:\n\t" oldinstr "\n662:\n" 			\
		      "  .align 4\n"					\
		      "  .long 661b\n"            /* label */		\
		      "  .long 663f\n"		  /* new instruction */	\
		      "  .byte %c0\n"             /* feature bit */	\
		      "  .byte 662b-661b\n"       /* sourcelen */	\
		      "  .byte 664f-663f\n"       /* replacementlen */	\
		      "663:\n\t" newinstr "\n664:\n"   /* replacement */\	
		      :: "i" (feature) : "memory")			  
#endif

/*
 * Alternative inline assembly with input.
 *
 * Pecularities:
 * No memory clobber here.
 * Argument numbers start with 1.
 * Best is to use constraints that are fixed size (like (%1) ... "r")
 * If you use variable sized constraints like "m" or "g" in the
 * replacement maake sure to pad to the worst case length.
 */
#ifndef __WINKVM__			 
#define alternative_input(oldinstr, newinstr, feature, input...)	\
	asm volatile ("661:\n\t" oldinstr "\n662:\n"			\
		      ".section .altinstructions,\"a\"\n"		\
		      "  .align 4\n"					\
		      "  .long 661b\n"            /* label */		\
		      "  .long 663f\n"		  /* new instruction */ \
		      "  .byte %c0\n"             /* feature bit */	\
		      "  .byte 662b-661b\n"       /* sourcelen */	\
		      "  .byte 664f-663f\n"       /* replacementlen */ 	\
		      ".previous\n"					\
		      ".section .altinstr_replacement,\"ax\"\n"		\
		      "663:\n\t" newinstr "\n664:\n"   /* replacement */\
		      ".previous" :: "i" (feature), ##input)
#else
#define alternative_input(oldinstr, newinstr, feature, input...)			
#endif

#ifndef __WINKVM__			  
/* Like alternative_input, but with a single output argument */
#define alternative_io(oldinstr, newinstr, feature, output, input...) \
	asm volatile ("661:\n\t" oldinstr "\n662:\n"			\
		      ".section .altinstructions,\"a\"\n"		\
		      "  .align 4\n"					\
		      "  .long 661b\n"            /* label */		\
		      "  .long 663f\n"		  /* new instruction */	\
		      "  .byte %c[feat]\n"        /* feature bit */	\
		      "  .byte 662b-661b\n"       /* sourcelen */	\
		      "  .byte 664f-663f\n"       /* replacementlen */	\
		      ".previous\n"					\
		      ".section .altinstr_replacement,\"ax\"\n"		\
		      "663:\n\t" newinstr "\n664:\n"   /* replacement */ \
		      ".previous" : output : [feat] "i" (feature), ##input)
#else
/* Like alternative_input, but with a single output argument */
#define alternative_io(oldinstr, newinstr, feature, output, input...)			 
#endif
			  
/*
 * use this macro(s) if you need more than one output parameter
 * in alternative_io
 */
#define ASM_OUTPUT2(a, b) a, b

/*
 * Alternative inline assembly for SMP.
 *
 * The LOCK_PREFIX macro defined here replaces the LOCK and
 * LOCK_PREFIX macros used everywhere in the source tree.
 *
 * SMP alternatives use the same data structures as the other
 * alternatives and the X86_FEATURE_UP flag to indicate the case of a
 * UP system running a SMP kernel.  The existing apply_alternatives()
 * works fine for patching a SMP kernel for UP.
 *
 * The SMP alternative tables can be kept after boot and contain both
 * UP and SMP versions of the instructions to allow switching back to
 * SMP at runtime, when hotplugging in a new CPU, which is especially
 * useful in virtualized environments.
 *
 * The very common lock prefix is handled as special case in a
 * separate table which is a pure address list without replacement ptr
 * and size information.  That keeps the table sizes small.
 */

#ifdef CONFIG_SMP
#ifndef __WINKVM__			  
#define LOCK_PREFIX \
		".section .smp_locks,\"a\"\n"	\
		"  .align 4\n"			\
		"  .long 661f\n" /* address */	\
		".previous\n"			\
	       	"661:\n\tlock; "
#else
#define LOCK_PREFIX \
		"  .align 4\n"	\
		"  .long 661f\n" /* address */	\
	    "661:\n\tlock; "
#endif			  
#else /* ! CONFIG_SMP */
#define LOCK_PREFIX ""
#endif

struct paravirt_patch_site;
#ifdef CONFIG_PARAVIRT
void apply_paravirt(struct paravirt_patch_site *start,
		    struct paravirt_patch_site *end);
#else
static inline void
apply_paravirt(struct paravirt_patch_site *start,
	       struct paravirt_patch_site *end)
{}
#define __parainstructions	NULL
#define __parainstructions_end	NULL
#endif

extern void text_poke(void *addr, unsigned char *opcode, int len);

#endif /* _I386_ALTERNATIVE_H */
