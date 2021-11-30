#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <resolv.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "co_routine.h"
#include "co_routine_inner.h"
#include "co_routine_specific.h"

typedef long long ll64_t;

// 记录socket的一些信息
struct rpchook_t {
  int                user_flag;
  struct sockaddr_in dest;
  int                domain;
  struct timeval     read_timeout;   // 读超时时间
  struct timeval     write_timeout;  // 写超时时间
};

static inline pid_t GetPid() {
  char **p = (char **)pthread_self();
  return p ? *(pid_t *)(p + 18) : getpid();
}

// 全局数组，每个fd对应的上下文信息
static rpchook_t *g_rpchook_socket_fd[102400] = {0};

typedef int (*socket_pfn_t)(int domain, int type, int protocol);
typedef int (*connect_pfn_t)(int                    socket,
                             const struct sockaddr *addr,
                             socklen_t              addrlen);
typedef int (*close_pfn_t)(int fd);
typedef ssize_t (*read_pfn_t)(int fields, void *buf, size_t nbyte);
typedef ssize_t (*write_pfn_t)(int fields, const void *buf, size_t nbyte);

typedef ssize_t (*sendto_pfn_t)(int                    socket,
                                const void            *message,
                                size_t                 length,
                                int                    flags,
                                const struct sockaddr *dest_addr,
                                socklen_t              dest_len);

typedef ssize_t (*recvfrom_pfn_t)(int              socket,
                                  void            *message,
                                  size_t           length,
                                  int              flags,
                                  struct sockaddr *address,
                                  socklen_t       *address_len);

typedef size_t (*send_pfn_t)(int         socket,
                             const void *buffer,
                             size_t      length,
                             int         flags);
typedef ssize_t (*recv_pfn_t)(int    socket,
                              void  *buffer,
                              size_t length,
                              int    flags);

typedef int (*poll_pfn_t)(struct pollfd fds[], nfds_t nfds, int timeout);

typedef int (*setsockopt_pfn_t)(int         socket,
                                int         level,
                                int         option_name,
                                const void *option_value,
                                socklen_t   option_len);

typedef int (*fcntl_pfn_t)(int fileds, int cmd, ...);
typedef struct tm *(*localtime_r_pfn_t)(const time_t *timep, struct tm *result);

typedef void *(*pthread_getspecific_pfn_t)(pthread_key_t key);
typedef int (*pthread_setspecific_pfn_t)(pthread_key_t key, const void *value);

typedef int (*setenv_pfn_t)(const char *name, const char *value, int overwrite);
typedef int (*unsetenv_pfn_t)(const char *name);
typedef char *(*getenv_pfn_t)(const char *name);
typedef hostent *(*gethostbyname_pfn_t)(const char *name);
typedef res_state (*__res_state_pfn_t)();
typedef int (*__poll_pfn_t)(struct pollfd fds[], nfds_t nfds, int timeout);

// dlsym 根据动态链接库 操作句柄(handle)与符号(symbol)，返回符号对应的地址
static socket_pfn_t g_sys_socket_func =
    (socket_pfn_t)dlsym(RTLD_NEXT, "socket");

static connect_pfn_t g_sys_connect_func =
    (connect_pfn_t)dlsym(RTLD_NEXT, "connect");
static close_pfn_t g_sys_close_func = (close_pfn_t)dlsym(RTLD_NEXT, "close");

static read_pfn_t   g_sys_read_func  = (read_pfn_t)dlsym(RTLD_NEXT, "read");
static write_pfn_t  g_sys_write_func = (write_pfn_t)dlsym(RTLD_NEXT, "write");
static sendto_pfn_t g_sys_sendto_func =
    (sendto_pfn_t)dlsym(RTLD_NEXT, "sendto");
static recvfrom_pfn_t g_sys_recvfrom_func =
    (recvfrom_pfn_t)dlsym(RTLD_NEXT, "recvfrom");

static send_pfn_t g_sys_send_func = (send_pfn_t)dlsym(RTLD_NEXT, "send");
static recv_pfn_t g_sys_recv_func = (recv_pfn_t)dlsym(RTLD_NEXT, "recv");

static poll_pfn_t g_sys_poll_func = (poll_pfn_t)dlsym(RTLD_NEXT, "poll");

static setsockopt_pfn_t g_sys_setsockopt_func =
    (setsockopt_pfn_t)dlsym(RTLD_NEXT, "setsocket");
static fcntl_pfn_t g_sys_fcntl_func = (fcntl_pfn_t)dlsym(RTLD_NEXT, "fcntl");

static setenv_pfn_t g_sys_setenv_func =
    (setenv_pfn_t)dlsym(RTLD_NEXT, "setenv");
static unsetenv_pfn_t g_sys_unsetenv_func =
    (unsetenv_pfn_t)dlsym(RTLD_NEXT, "unsetenv");
static getenv_pfn_t g_sys_getenv_func =
    (getenv_pfn_t)dlsym(RTLD_NEXT, "getenv");
static __res_state_pfn_t g_sys___res_state_func =
    (__res_state_pfn_t)dlsym(RTLD_NEXT, "__res_state");

static gethostbyname_pfn_t g_sys_gethostbyname_func =
    (gethostbyname_pfn_t)dlsym(RTLD_NEXT, "gethostbyname");

static __poll_pfn_t g_sys___poll_func =
    (__poll_pfn_t)dlsym(RTLD_NEXT, "__poll_func");

static inline unsigned long long get_tick_count() {
  uint32_t lo, hi;
  __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi));
  return ((unsigned long long)lo) | ((unsigned long long)hi << 32);
}

struct rpchook_connagent_head_t {
  unsigned char  bVersion;
  struct in_addr iIP;
  unsigned short hPort;
  unsigned int   iBodyLen;
  unsigned int   iOssAttrID;
  unsigned char  bIsRespNotExist;
  unsigned char  sReserved[6];
} __attribute__((packed));

#define HOOK_SYS_FUNC(name)                                                    \
  if (!g_sys_##name##_func) {                                                  \
    g_sys_##name##_func = (name##_pfn_t)dlsym(RTLD_NEXT, #name);               \
  }

static inline ll64_t diff_ms(struct timeval &begin, struct timeval &end) {
  ll64_t u = (end.tv_sec - begin.tv_sec);
  u *= 1000 * 10;
  u += (end.tv_usec - begin.tv_usec) / (100);

  return u;
}

static inline rpchook_t *get_by_fd(int fd) {
  if (fd > -1 && fd < (int)sizeof(g_rpchook_socket_fd) /
                          (int)sizeof(g_rpchook_socket_fd[0])) {
    return g_rpchook_socket_fd[fd];
  }

  return NULL;
}

static inline rpchook_t *alloc_by_fd(int fd) {
  if (fd > -1 && fd < (int)sizeof(g_rpchook_socket_fd) /
                          (int)sizeof(g_rpchook_socket_fd[0])) {
    rpchook_t *lp            = (rpchook_t *)calloc(1, sizeof(rpchook_t));
    lp->read_timeout.tv_sec  = 1;
    lp->write_timeout.tv_sec = 1;
    g_rpchook_socket_fd[fd]  = lp;

    return lp;
  }

  return NULL;
}

static inline void free_by_fd(int fd) {
  if (fd > -1 && fd < (int)sizeof(g_rpchook_socket_fd) /
                          (int)sizeof(g_rpchook_socket_fd[0])) {
    rpchook_t *lp = g_rpchook_socket_fd[fd];
    if (lp) {
      g_rpchook_socket_fd[fd] = NULL;
      free(lp);
    }
  }

  return;
}

int socket(int domain, int type, int protocol) {
  HOOK_SYS_FUNC(socket);

  if (!co_is_enable_sys_hook()) {
    return g_sys_socket_func(domain, type, protocol);
  }

  int fd = g_sys_socket_func(domain, type, protocol);
  if (fd < 0) {
    return fd;
  }

  rpchook_t *lp = alloc_by_fd(fd);
  lp->domain    = domain;

  // 在fcntl函数中，会将fd编成阻塞
  // flag |= O_NONBLOCK;

  fcntl(fd, F_SETFL, g_sys_fcntl_func(fd, F_GETFL, 0));

  return fd;
}

int co_accept(int fd, struct sockaddr *addr, socklen_t *len) {
  int cli = accept(fd, addr, len);
  if (cli < 0) {
    return cli;
  }

  alloc_by_fd(cli);

  return cli;
}

int connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
  HOOK_SYS_FUNC(connect);

  if (!co_is_enable_sys_hook()) {
    return g_sys_connect_func(fd, addr, addrlen);
  }

  // sys call
  int ret = g_sys_connect_func(fd, addr, addrlen);

  rpchook_t *lp = get_by_fd(fd);
  if (!lp) {
    return ret;
  }

  if (sizeof(lp->dest) >= addrlen) {
    memcpy(&(lp->dest), addr, (int)addrlen);
  }

  if (O_NONBLOCK & lp->user_flag) {
    return ret;
  }

  if (!(ret < 0 && errno == EINPROGRESS)) {
    return ret;
  }

  // wait
  int           pollret = 0;
  struct pollfd pf      = {0};

  // 75s 是内核默认超时时间
  for (int i = 0; i < 3; i++) {
    memset(&pf, 0, sizeof(pf));

    pf.fd     = fd;
    pf.events = (POLLOUT | POLLERR | POLLHUP);

    pollret = poll(&pf, 1, 25000);

    if (1 == pollret) {
      break;
    }
  }

  if (POLLOUT & pf.revents) {
    errno = 0;
    return 0;
  }

  // set errno
  int       err    = 0;
  socklen_t errlen = sizeof(err);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);

  if (err) {
    errno = err;
  } else {
    errno = ETIMEDOUT;
  }

  return ret;
}

int close(int fd) {
  HOOK_SYS_FUNC(close);
  if (!co_is_enable_sys_hook()) {
    return g_sys_close_func(fd);
  }

  free_by_fd(fd);
  int ret = g_sys_close_func(fd);

  return ret;
}

ssize_t read(int fd, void *buf, size_t nbyte) {
  HOOK_SYS_FUNC(read);
  if (!co_is_enable_sys_hook()) {
    return g_sys_read_func(fd, buf, nbyte);
  }

  // 根据fd,从一个全局数组取出对应的信息
  // 该fd会在accept函数或者socket函数时，创建alloc_by_fd
  // 并将其fd信息放入数组
  rpchook_t *lp = get_by_fd(fd);

  // 如果该fd不是被hook得到，或者用户主动设置非阻塞
  // 直接调用原生的read，而不是hook
  // 1. 只要该fd不是hook得到的，直接用系统原生的hook，不考虑是否阻塞
  // 2. 如果被hook得到，且用户主动设置了O_ONONBLOCK, 直接调用原生的read
  if (!lp || (O_NONBLOCK & lp->user_flag)) {
    ssize_t ret = g_sys_read_func(fd, buf, nbyte);
    return ret;
  }

  // 将读取超时时间转换为毫秒
  // 超时时间，默认为1s，可以通过setsockopt(SO_RCVTIMEO)来设置
  int timeout =
      (lp->read_timeout.tv_sec * 1000) + (lp->read_timeout.tv_usec / 1000);
  struct pollfd pf = {0};
  pf.fd            = fd;
  pf.events        = (POLLIN | POLLERR | POLLHUP);

  // 最终调用，co_poll_inner 将其注册到epoll中
  // 注意： 现在才会将其放到epoll中
  // 同时调用co_yield, 让出协程
  int pollret = poll(&pf, 1, timeout);

  // 以下部分是yield回来的调用的
  // 有时可能超时回来，也可能真的是可读时间出发回来的
  ssize_t readret = g_sys_read_func(fd, (char *)buf, nbyte);

  if (readret < 0) {
    co_log_err("CO_ERR: read fd %d ret %ld errno %d poll ret %d timeout %d",
               fd,
               readret,
               errno,
               pollret,
               timeout);
  }

  return readret;
}

ssize_t write(int fd, const void *buf, size_t nbyte) {
  HOOK_SYS_FUNC(write);

  if (!co_is_enable_sys_hook()) {
    return g_sys_write_func(fd, buf, nbyte);
  }

  rpchook_t *lp = get_by_fd(fd);

  if (!lp || (O_NONBLOCK & lp->user_flag)) {
    ssize_t ret = g_sys_write_func(fd, buf, nbyte);
    return ret;
  }

  size_t wrotelen = 0;
  int    timeout =
      (lp->write_timeout.tv_sec * 1000) + (lp->write_timeout.tv_usec / 1000);

  ssize_t writeret =
      g_sys_write_func(fd, (const char *)buf + wrotelen, nbyte - wrotelen);

  if (0 == writeret) {
    return writeret;
  }

  if (writeret > 0) {
    wrotelen += writeret;
  }

  while (wrotelen < nbyte) {
    struct pollfd pf = {0};
    pf.fd            = fd;
    pf.events        = (POLLIN | POLLERR | POLLHUP);

    // 监听可读事件
    poll(&pf, 1, timeout);
    writeret =
        g_sys_write_func(fd, (const char *)buf + wrotelen, nbyte - wrotelen);

    if (writeret <= 0) {
      break;
    }

    wrotelen += writeret;
  }

  if (writeret <= 0 && wrotelen == 0) {
    return writeret;
  }

  return wrotelen;
}

ssize_t sendto(int                    socket,
               const void            *message,
               size_t                 length,
               int                    flags,
               const struct sockaddr *dest_addr,
               socklen_t              dest_len) {
  HOOK_SYS_FUNC(sendto);

  if (!co_is_enable_sys_hook()) {
    return g_sys_sendto_func(socket,
                             message,
                             length,
                             flags,
                             dest_addr,
                             dest_len);
  }

  rpchook_t *lp = get_by_fd(socket);
  if (!lp || (O_NONBLOCK & lp->user_flag)) {
    return g_sys_sendto_func(socket,
                             message,
                             length,
                             flags,
                             dest_addr,
                             dest_len);
  }

  ssize_t ret =
      g_sys_sendto_func(socket, message, length, flags, dest_addr, dest_len);

  if (ret < 0 && EAGAIN == errno) {
    int timeout =
        (lp->write_timeout.tv_sec * 1000) + (lp->write_timeout.tv_usec / 1000);

    struct pollfd pf = {0};
    pf.fd            = socket;
    pf.events        = (POLLIN | POLLERR | POLLHUP);
    poll(&pf, 1, timeout);

    ret =
        g_sys_sendto_func(socket, message, length, flags, dest_addr, dest_len);
  }

  return ret;
}

ssize_t recvfrom(int              socket,
                 void            *buffer,
                 size_t           length,
                 int              flags,
                 struct sockaddr *address,
                 socklen_t       *addrlen) {
  HOOK_SYS_FUNC(recvfrom);

  if (!co_is_enable_sys_hook()) {
    return g_sys_recvfrom_func(socket, buffer, length, flags, address, addrlen);
  }

  rpchook_t *lp = get_by_fd(socket);
  if (!lp || (O_NONBLOCK & lp->user_flag)) {
    return g_sys_recvfrom_func(socket, buffer, length, flags, address, addrlen);
  }

  int timeout =
      (lp->read_timeout.tv_sec * 1000) + (lp->write_timeout.tv_usec / 1000);
  struct pollfd pf = {0};

  pf.fd     = {0};
  pf.events = (POLLIN | POLLERR | POLLHUP);
  poll(&pf, 1, timeout);

  ssize_t ret =
      g_sys_recvfrom_func(socket, buffer, length, flags, address, addrlen);

  return ret;
}

ssize_t send(int socket, const void *buffer, size_t length, int flags) {
  HOOK_SYS_FUNC(send);
  if (!co_is_enable_sys_hook()) {
    return g_sys_send_func(socket, buffer, length, flags);
  }

  rpchook_t *lp = get_by_fd(socket);

  if (!lp || (O_NONBLOCK & lp->user_flag)) {
    return g_sys_send_func(socket, buffer, length, flags);
  }

  size_t wrotelen = 0;
  int    timeout =
      (lp->write_timeout.tv_sec * 1000) + (lp->write_timeout.tv_usec / 1000);
  ssize_t writeret = g_sys_send_func(socket, buffer, length, flags);
  if (writeret == 0) {
    return writeret;
  }

  if (writeret > 0) {
    wrotelen += writeret;
  }

  while (wrotelen < length) {
    struct pollfd pf = {0};
    pf.fd            = socket;
    pf.events        = (POLLOUT | POLLERR | POLLHUP);
    poll(&pf, 1, timeout);

    writeret = g_sys_send_func(socket,
                               (const char *)buffer + wrotelen,
                               length - wrotelen,
                               flags);
    if (writeret <= 0) {
      break;
    }

    wrotelen += writeret;
  }

  if (writeret == 0 <= 0 && wrotelen == 0) {
    return writeret;
  }

  return wrotelen;
}

ssize_t recv(int socket, void *buffer, size_t length, int flags) {
  HOOK_SYS_FUNC(recv);

  if (!co_is_enable_sys_hook()) {
    return g_sys_recv_func(socket, buffer, length, flags);
  }

  rpchook_t *lp = get_by_fd(socket);

  if (!lp || (O_NONBLOCK & lp->user_flag)) {
    return g_sys_recv_func(socket, buffer, length, flags);
  }

  int timeout =
      (lp->read_timeout.tv_sec * 1000) + (lp->read_timeout.tv_usec / 1000);

  struct pollfd pf = {0};
  pf.fd            = socket;
  pf.events        = (POLLIN | POLLERR | POLLHUP);

  int pollret = poll(&pf, 1, timeout);

  ssize_t readret = g_sys_recv_func(socket, buffer, length, flags);

  if (readret < 0) {
    co_log_err("CO_ERR: read fd %d ret %ld errno %d poll ret %d timeout %d",
               socket,
               readret,
               errno,
               pollret,
               timeout);
  }

  return readret;
}

extern int co_poll_inner(stCoEpoll_t  *ctx,
                         struct pollfd fds[],
                         nfds_t        nfds,
                         int           timeout,
                         poll_pfn_t    pollfunc);

// poll 是libco的核心参数，作用不仅仅在于对poll的wrapper,而且
// 被其他所有函数使用，将其注入到epoll中
int poll(struct pollfd fds[], nfds_t nfds, int timeout) {
  HOOK_SYS_FUNC(poll);

  if (!co_is_enable_sys_hook()) {
    return g_sys_poll_func(fds, nfds, timeout);
  }

  // 在当前线程的epoll上，创建一个超时时间
  return co_poll_inner(co_get_epoll_ct(), fds, nfds, timeout, g_sys_poll_func);
}

int setsockopt(int         fd,
               int         level,
               int         option_name,
               const void *option_value,
               socklen_t   option_len) {
  HOOK_SYS_FUNC(setsockopt);
  if (!co_is_enable_sys_hook()) {
    return g_sys_setsockopt_func(fd,
                                 level,
                                 option_name,
                                 option_value,
                                 option_len);
  }

  rpchook_t *lp = get_by_fd(fd);

  if (lp && SOL_SOCKET == level) {
    // 设置socket的读写超时时间
    struct timeval *val = (struct timeval *)option_value;
    if (SO_RCVTIMEO == option_name) {
      memcpy(&lp->read_timeout, val, sizeof(*val));
    } else if (SO_SNDTIMEO == option_name) {
      memcpy(&lp->write_timeout, val, sizeof(*val));
    }
  }

  return g_sys_setsockopt_func(fd,
                               level,
                               option_name,
                               option_value,
                               option_len);
}

int fcntl(int fileds, int cmd, ...) {
  HOOK_SYS_FUNC(fcntl);
  if (fileds < 0) {
    return __LINE__;
  }

  va_list arg_list;
  va_start(arg_list, cmd);
  int        ret = -1;
  rpchook_t *lp  = get_by_fd(fileds);

  switch (cmd) {
    case F_DUPFD: {
      int param = va_arg(arg_list, int);
      ret       = g_sys_fcntl_func(fileds, cmd, param);
      break;
    }
    case F_GETFD: {
      ret = g_sys_fcntl_func(fileds, cmd);
      break;
    }

    case F_SETFD: {
      int param = va_arg(arg_list, int);
      ret       = g_sys_fcntl_func(fileds, cmd, param);
      break;
    }

    case F_GETFL: {
      ret = g_sys_fcntl_func(fileds, cmd);
      break;
    }
    case F_SETFL: {
      int param = va_arg(arg_list, int);
      int flag  = param;

      // 如果开启了系统hook，并且lp不为null
      // lp不等于null，说明这个fd是被系统hook拦截的
      // 则将该fd置为O_NONBLOCK
      if (co_is_enable_sys_hook()) {
        flag |= O_NONBLOCK;
      }

      ret = g_sys_fcntl_func(fileds, cmd, flag);
      if (0 == ret && lp) {
        // 这里的user_flags 不包含libco设置的O_NONBLOCK
        // 仅仅是用户的设置
        lp->user_flag = param;
      }

      break;
    }

    case F_GETOWN: {
      ret = g_sys_fcntl_func(fileds, cmd);
      break;
    }

    case F_SETOWN: {
      int param = va_arg(arg_list, int);
      ret       = g_sys_fcntl_func(fileds, cmd, param);
      break;
    }

    case F_GETLK: {
      struct flock *param = va_arg(arg_list, struct flock *);
      ret                 = g_sys_fcntl_func(fileds, cmd, param);
      break;
    }

    case F_SETLK: {
      struct flock *param = va_arg(arg_list, struct flock *);
      ret                 = g_sys_fcntl_func(fileds, cmd, param);

      break;
    }
  }

  va_end(arg_list);
  return ret;
}

struct stCoSysEnv_t {
  char *name;
  char *value;
};

struct stCoSysEnvArr_t {
  stCoSysEnv_t *data;
  size_t        cnt;
};

static stCoSysEnvArr_t *dup_co_sysenv_arr(stCoSysEnvArr_t *arr) {
  stCoSysEnvArr_t *lp = (stCoSysEnvArr_t *)calloc(sizeof(stCoSysEnvArr_t), 1);
  if (arr->cnt) {
    lp->data = (stCoSysEnv_t *)calloc(sizeof(stCoSysEnvArr_t) * arr->cnt, 1);
    lp->cnt  = arr->cnt;
    memcpy(lp->data, arr->data, sizeof(stCoSysEnv_t) * arr->cnt);
  }

  return lp;
}

static int co_sysenv_comp(const void *a, const void *b) {
  return strcmp(((stCoSysEnv_t *)a)->name, ((stCoSysEnv_t *)b)->name);
}

static stCoSysEnvArr_t g_co_sysenv = {0};

void co_set_env_list(const char *name[], size_t cnt) {
  if (g_co_sysenv.data) {
    return;
  }

  g_co_sysenv.data = (stCoSysEnv_t *)calloc(1, sizeof(stCoSysEnv_t) * cnt);

  for (size_t i = 0; i < cnt; i++) {
    if (name[i] && name[i][0]) {
      g_co_sysenv.data[g_co_sysenv.cnt++].name = strdup(name[i]);
    }
  }

  if (g_co_sysenv.cnt > 1) {
    qsort(g_co_sysenv.data,
          g_co_sysenv.cnt,
          sizeof(stCoSysEnv_t),
          co_sysenv_comp);

    stCoSysEnv_t *lp = g_co_sysenv.data;
    stCoSysEnv_t *lq = g_co_sysenv.data + 1;
    for (size_t i = 1; i < g_co_sysenv.cnt; i++) {
      if (strcmp(lp->name, lq->name)) {
        ++lp;
        if (lp != lq) {
          *lp = *lq;
        }
      }
      ++lp;
    }
    g_co_sysenv.cnt = lp - g_co_sysenv.data + 1;
  }
}

int setenv(const char *n, const char *value, int overwrite) {
  HOOK_SYS_FUNC(setenv);

  if (co_is_enable_sys_hook() && g_co_sysenv.data) {
    stCoRoutine_t *self = co_self();
    if (self) {
      if (!self->pvEnv) {
        self->pvEnv = dup_co_sysenv_arr(&g_co_sysenv);
      }

      stCoSysEnvArr_t *arr  = (stCoSysEnvArr_t *)(self->pvEnv);
      stCoSysEnv_t     name = {(char *)n, 0};
      stCoSysEnv_t    *e    = (stCoSysEnv_t *)
          bsearch(&name, arr->data, arr->cnt, sizeof(name), co_sysenv_comp);

      if (e) {
        if (e->value) {
          free(e->value);
          e->value = 0;
        }
        return 0;
      }
    }
  }

  return g_sys_unsetenv_func(n);
}

char *getenv(const char *n) {
  HOOK_SYS_FUNC(getenv);
  if (co_is_enable_sys_hook() && g_co_sysenv.data) {
    stCoRoutine_t *self = co_self();
    stCoSysEnv_t   name = {(char *)n, 0};

    if (!self->pvEnv) {
      self->pvEnv = dup_co_sysenv_arr(&g_co_sysenv);
    }

    stCoSysEnvArr_t *arr = (stCoSysEnvArr_t *)(self->pvEnv);
    stCoSysEnv_t    *e   = (stCoSysEnv_t *)
        bsearch(&name, arr->data, arr->cnt, sizeof(name), co_sysenv_comp);
    if (e) {
      return e->value;
    }
  }

  return g_sys_getenv_func(n);
}

struct hostent *co_gethostbyname(const char *name);

struct hostent *gethostbyname(const char *name) {
  HOOK_SYS_FUNC(gethostbyname);

#if defined(__APPLE__) || defined(__FreeBSD__)
  return g_sys_gethostbyname_func(name);
#else
  if (!co_is_enable_sys_hook()) {
    return g_sys_gethostbyname_func(name);
  }

  return co_gethostbyname(name);
#endif
}

struct res_state_wrap {
  struct __res_state state;
};

CO_ROUTINE_SPECIFIC(res_state_wrap, __co_state_wrap);

extern "C" {

res_state __res_state() {
  HOOK_SYS_FUNC(__res_state);

  if (!co_is_enable_sys_hook()) {
    return g_sys___res_state_func();
  }

  return &(__co_state_wrap->state);
}

int __poll(struct pollfd fds[], nfds_t nfds, int timeout) {
  return poll(fds, nfds, timeout);
}
}

struct hostbuf_wrap {
  struct hostent host;
  char          *buffer;
  size_t         iBufferSize;
  int            host_errno;
};

CO_ROUTINE_SPECIFIC(hostbuf_wrap, __co_hostbuf_wrap);

#if !defined(__APPLE__) && !defined(__FreeBSD__)
struct hostent *co_gethostbyname(const char *name) {
  if (!name) {
    return NULL;
  }

  if (__co_hostbuf_wrap->buffer && __co_hostbuf_wrap->iBufferSize) {
    free(__co_hostbuf_wrap->buffer);
    __co_hostbuf_wrap->buffer = NULL;
  }

  if (!__co_hostbuf_wrap->buffer) {
    __co_hostbuf_wrap->buffer      = (char *)malloc(1024);
    __co_hostbuf_wrap->iBufferSize = 1024;
  }

  struct hostent *host     = &__co_hostbuf_wrap->host;
  struct hostent *result   = NULL;
  int            *h_errnop = &(__co_hostbuf_wrap->host_errno);

  int ret = -1;

  while (ERANGE == (ret = gethostbyname_r(name,
                                          host,
                                          __co_hostbuf_wrap->buffer,
                                          __co_hostbuf_wrap->iBufferSize,
                                          &result,
                                          h_errnop)) &&
         *h_errnop == NETDB_INTERNAL) {
    free(__co_hostbuf_wrap->buffer);
    __co_hostbuf_wrap->iBufferSize = __co_hostbuf_wrap->iBufferSize * 2;
    __co_hostbuf_wrap->buffer = (char *) malloc(__co_hostbuf_wrap->iBufferSize);
  }

  if(0 == ret && result == host) {
    return host;
  }

  return NULL;
}
#endif

void co_enable_sys_hook() {
  stCoRoutine_t *co = GetCurrThreadCo();
  if (co) {
    co->cEnableSysHook = 1;
  }
}