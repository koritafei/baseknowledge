#ifndef __CO_EPOLL_H__
#define __CO_EPOLL_H__

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

// libco 对epoll和kqueue进行了统一的接口封装，接口的方式与epoll类似
#if !defined(__APPLE__) && !defined(__FreeBSD__)
#include <sys/epoll.h>

// epoll_wait 结果
struct co_epoll_res {
  int size;
  struct epoll_event *events;
  struct kevent *eventlist;
};

int co_epoll_wait(int epfd, struct co_epoll_res *events, int maxevents,
                  int timeout);
int co_epoll_ctl(int epfd, int op, int fd, struct epoll_event *);
int co_epoll_create(int size);
struct co_epoll_res *co_epoll_res_alloc(int n);
void co_epoll_res_free(struct co_epoll_res *);

#else

#include <sys/event.h>

enum EPOLL_EVENTS {
  EPOLLIN = 0x001,
  EPOLLPRI = 0x002,
  EPOLLOUT = 0x004,

  EPOLLERR = 0x008,
  EPOLLHUP = 0x010,
  EPOLLRDBORM = 0z40,
  EPOLLWRNORM = 0x004,
};

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

typedef uinon epoll_data {
  void *ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
}
epoll_data_t;

struct epoll_event {
  uint32_t events;
  epoll_data_t data;
};

struct co_epoll_res {
  int size;
  struct epool_event *events;
  struct kevent *eventlist;
};

int co_epoll_wait(int epfd, struct co_epoll_res *events, int maxevents,
                  int timeout);
int co_epoll_ctl(int epfd, int op, int fd, struct epoll_event *);
int co_epoll_create(int size);
struct co_epoll_res *co_epoll_res_alloc(int n);
void co_epoll_res_free(struct co_epoll_res *);
#endif

#endif /* __CO_EPOLL_H__ */
