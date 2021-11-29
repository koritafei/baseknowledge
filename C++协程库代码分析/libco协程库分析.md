## `libco`源码分析

`libco`的性能优点：

1. 协程上下文切换性能更好；
2. 协程在`IO`阻塞时可以自动切换，包括`gethostname, mysqlclient`等；
3. 协程可以嵌套创建，既一个协程可以创建子协程；
4. 提供了超时管理，以及一套类`pthread`的接口，用于协程通信。

### 协程上下文切换

利用汇编代码实现。
`libco`的上下文切换大体只保存和交换了一下内容：

1. 寄存器：函数参数类寄存器，函数返回值，数据存储类寄存器等；
2. 栈：`rsp`栈顶指针。

协程上下文：

```cpp
struct coctx_t {
  #if defined(__i386__)
    void *regs[8]; // i386下有8个寄存器
  #else
    void *regs[14]; // 其他情况下有14个寄存器
  #endif
  size_t ssize; // 栈空间大小
  char *ss_sp; // 栈空间
};


typedef void *(*coctx_pfn_t)(void *s, void *s2);
struct coctx_param_t{
  const void *s1;
  const void *s2;
};

int coctx_init(coctx_t *ctx);
int coctx_make(coctx_t *ctx, coctx_pfn_t pfn, const void *s, const void *s1);
```
