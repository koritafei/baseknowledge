#ifndef __CO_ROUTINE_H__
#define __CO_ROUTINE_H__

#include <pthread.h>
#include <stdint.h>
#include <sys/poll.h>

// struct
struct stCoRoutine_t;
struct stShareStack_t;  // 共享栈

/**
 * @brief 协程属性，用于每个协程自定义一些属性
 * */
struct stCoRoutineAttr_t {
  int stack_size;  // 如果是共享栈模式则无需制定，否则必须指定
  stShareStack_t *share_stack;

  stCoRoutineAttr_t() {
    stack_size  = 128 * 1024;  // 默认为128KB
    share_stack = NULL;        // 默认不是共享栈
  }
} __attribute__((packed));

struct stCoEpoll_t;
typedef int (*pfn_co_eventloop_t)(void *);
typedef void *(*pfn_co_routine_t)(void *);

/**
 * @brief  创建一个协程
 * @param  co               协程调度器
 * @param  attr             协程属性
 * @param  routine          协程运行函数
 * @param  args             参数
 * @return int
 * */
int co_create(stCoRoutine_t          **co,
              const stCoRoutineAttr_t *attr,
              void *(*routine)(void *),
              void *args);

/**
 * @brief 提交运行协程
 * @param  co              协程调度器
 * @return int
 * */
int co_resume(stCoRoutine_t *co);

/**
 * @brief 挂起协程
 * @param  co           协程调度器
 * */
void co_yield(stCoRoutine_t *co);

/**
 * @brief ct = current thread 挂起当前线程的所有协程
 * */
void co_yield_ct();

/**
 * @brief 释放所有协程
 * @param  co              协程调度器
 * */
void co_release(stCoRoutine_t *co);

/**
 * @brief 返回当前的协程调度器
 * @return stCoRoutine_t*
 * */
stCoRoutine_t *co_self();

/**
 * @brief
 * @param  ctx              My Pan doc
 * @param  fds              My Pan doc
 * @param  nfds             My Pan doc
 * @param  timeout_ms       My Pan doc
 * @return int
 * */
int co_poll(stCoEpoll_t *ctx, struct pollfd *fds, nfds_t nfds, int timeout_ms);

/**
 * @brief
 * @param  ctx              My Pan doc
 * @param  pfn              My Pan doc
 * @param  arg              My Pan doc
 * */
void co_eventloop(stCoEpoll_t *ctx, pfn_co_eventloop_t pfn, void *arg);

// specific
/**
 * @brief
 * @param  key              My Pan doc
 * @param  value            My Pan doc
 * @return int
 * */
int co_setspecific(pthread_key_t key, const void *value);

/**
 * @brief
 * @param  key              My Pan doc
 * */
void *co_getspecific(pthread_key_t key);

// event

/**
 * @brief 获取当前线程的所有协程事件
 * @return stCoEpoll_t*
 * */
stCoEpoll_t *co_get_epoll_ct();

// hook sys call
void co_enable_hook_sys();
void co_disable_hook_sys();
bool co_is_enable_sys_hook();

// sync
struct stCoCond_t;
stCoCond_t *co_cond_alloc();
int         co_cond_free(stCoCond_t *cc);
int         co_cond_signal(stCoCond_t *);
int         co_cond_broadcast(stCoCond_t *);
int         co_cond_timedwait(stCoCond_t *, int timeout_ms);

// share stack
stShareStack_t *co_alloc_sharestack(int iCount, int iStackSize);

void co_set_env_list(const char *name[], size_t cnt);
void co_log_err(const char *fmt, ...);
#endif /* __CO_ROUTINE_H__ */
