#ifndef __CO_ROUTINE_INNER_H__
#define __CO_ROUTINE_INNER_H__

#include "co_routine.h"
#include "coctx.h"

struct stCoRoutineEnv_t;

struct stCoSpec_t {
  void *value;
};

/**
 * @brief 共享栈，
 * 一个进程或线程的地址，是从高位到低位安排数据的，所以stack_bp是栈底，stack_buffer是栈顶
 * */
struct stStackMem_t {
  stCoRoutine_t *occupy_co; // 当前正在使用该共享栈的协程
  int stack_size;           // 栈的大小
  char *stack_bp;           // stack_buffer + stack_size 栈底
  char *stack_buffer;       // 栈的内容，也就是栈顶
};

/**
 * @brief 共享栈数组
 * */
struct stShareStack_t {
  unsigned int alloc_idx;    // 目前正在使用共享栈的index
  int stack_size;            // 共享栈大小 一个stStackMem_t*的大小
  int count;                 // 共享栈的大小
  stStackMem_t **stackarray; // 栈的内容，每个栈的元素为 stStackMem_t *
};

// 协程
struct stCoRoutine_t {
  stCoRoutineEnv_t *env; // 协程运行环境，可以认为协程所属的协程管理器
  pfn_co_routine_t pfn; // 协程对应的函数
  void *arg;            // 协程参数
  coctx_t ctx;          // 协程上下文，包括栈和寄存器

  // 以下用char 代表了bool 语义，节省空间
  char cStart;         // 是否开始运行
  char cEnd;           // 是否已经结束
  char cIsMain;        // 是否为主协程
  char cEnableSysHook; // 是否打开钩子标识，默认关闭
  char cIsShareStack;  // 是否采用共享栈

  void *pvEnv;

  stStackMem_t *stack_mem; // 栈内存
  char *stack_sp;
  unsigned int save_size; // save_buffer的长度
  char *sace_buffer; // 当协程挂起时，栈的内容会暂时存到save_buffer中

  stCoSpec_t aSpec[1024];
};

// env
/**
 * @brief
 * */
void co_init_curr_thread_env();
stCoRoutineEnv_t *co_get_curr_thread_env();

// coroutine
void co_free(stCoRoutine_t *co);
void co_yield_env(stCoRoutineEnv_t *env);

// func
struct stTimeout_t;
struct stTimeoutItem_t;

stTimeout_t *AllocTimeout(int iSize);
void FreeTimeout(stTimeout_t *apTimeout);
int AddTimeout(stTimeout_t *apTimeout, stTimeoutItem_t *apItem,
               uint64_t allNow);

struct stCoEpool_t;
stCoEpool_t *AllocEpoll();
void FreeEpoll(stCoEpool_t *ctx);

stCoRoutine_t *GetCurrThreadCo();

void SetEpoll(stCoRoutineEnv_t *env, stCoEpoll_t *ev);

typedef void (*pfnCoRoutineFunc_t)();

#endif /* __CO_ROUTINE_INNER_H__ */
