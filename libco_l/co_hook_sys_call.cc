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
                                socklen_t  *option_len);

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
      (lp->read_timeout.tv_sec * 1000) + (lp->write_timeout.tv_usec / 100);
  struct pollfd pf = {0};

  pf.fd     = {0};
  pf.events = (POLLIN | POLLERR | POLLHUP);
  poll(&pf, 1, timeout);

  ssize_t ret =
      g_sys_recvfrom_func(socket, buffer, length, flags, address, addrlen);

  return ret;
}
