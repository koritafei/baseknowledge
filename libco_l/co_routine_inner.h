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

#endif /* __CO_ROUTINE_INNER_H__ */
