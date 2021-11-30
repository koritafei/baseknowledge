#ifndef __COCTX_H__
#define __COCTX_H__

#include <stdio.h>

typedef void *(*coctx_pfn_t)(const void *s1, const void *s2);

struct coctx_param_t {
  const void *s1;
  const void *s2;
};

struct coctx_t {
#if defined(__i386__)
  void *regs[8];  //  x86使用8个寄存器
#else
  void *regs[14];  // 其他寄存器保留14个
#endif

  size_t ss_size;  // 栈大小
  char  *ss_sp;    // 栈
};

int coctx_init(coctx_t *ctx);
int coctx_make(coctx_t *ctx, coctx_pfn_t pfn, const void *s1, const void *s2);
#endif /* __COCTX_H__ */
