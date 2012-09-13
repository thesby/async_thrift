/** @file
 * @brief some IO system call wrappers that ignore EINTR
 * @author zhangyafeikimi@gmail.com
 * @date
 * @version
 *
 * Posix
 */
#ifndef EINTR_WRAPPER_H
#define EINTR_WRAPPER_H

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

  static inline int close_no_eintr(int fd)
  {
    int ret;
    do ret = close(fd);
    while (ret == -1 && errno == EINTR);
    return ret;
  }

  static inline ssize_t read_no_eintr(int fd, void * buf, size_t count)
  {
    ssize_t ret;
    do ret = read(fd, buf, count);
    while (ret == -1 && errno == EINTR);
    return ret;
  }

  static inline ssize_t write_no_eintr(int fd, const void * buf, size_t count)
  {
    ssize_t ret;
    do ret = write(fd, buf, count);
    while (ret == -1 && errno == EINTR);
    return ret;
  }

  static inline int open2_no_eintr(const char * pathname, int flags)
  {
    ssize_t ret;
    do ret = open(pathname, flags);
    while (ret == -1 && errno == EINTR);
    return ret;
  }

  static inline int open3_no_eintr(const char * pathname, int flags, mode_t mode)
  {
    ssize_t ret;
    do ret = open(pathname, flags, mode);
    while (ret == -1 && errno == EINTR);
    return ret;
  }

#ifdef __cplusplus
}
#endif

#endif/* EINTR_WRAPPER_H */
