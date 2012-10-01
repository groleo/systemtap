#ifndef _K_SYMS_H_
#define _K_SYMS_H_

#ifdef __powerpc64__
#define KERNEL_RELOC_SYMBOL ".__start"
#else
#define KERNEL_RELOC_SYMBOL "_stext"
#endif

#endif
