/** @file
 * @brief process statistics
 * @author zhangyafeikimi@gmail.com
 * @date
 * @version
 *
 */
#include "process_stat.h"

#ifndef _WIN32
#include "eintr_wrapper.h"
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

int get_processor_number()
{
  static int res = -1;

  if (-1 == res)
  {
    res = sysconf(_SC_NPROCESSORS_ONLN);

    if (-1 == res)
      res = 1;
  }
  return res;
}

static int64_t timeval_to_microseconds(const struct timeval * tv)
{
  return tv->tv_sec * 1000000 + tv->tv_usec;
}

int get_cpu_usage()
{
  static int64_t last_time = 0;
  static int64_t last_system_time = 0;
  static int processor_count = -1;

  struct timeval now;
  struct rusage usage;
  int64_t system_time, sys_time;
  int64_t system_time_delta, time_delta;
  int cpu;
  int retval;


  if (-1 == processor_count)
    processor_count = get_processor_number();

  retval = gettimeofday(&now, NULL);

  if (retval)
    return -1;

  retval = getrusage((int)RUSAGE_SELF, &usage);

  if (retval)
    return -1;

  system_time = (timeval_to_microseconds(&usage.ru_stime) +
      timeval_to_microseconds(&usage.ru_utime)) / processor_count;
  sys_time = timeval_to_microseconds(&now);

  if ((0 == last_system_time) || (0 == last_time))
  {
    /* First call, just set the last values. */
    last_system_time = system_time;
    last_time = sys_time;
    return 0;
  }

  system_time_delta = system_time - last_system_time;
  time_delta = sys_time - last_time;

  /* DCHECK(time_delta != 0); */
  if (0 == time_delta)
    return 0;

  /* We add time_delta / 2 so the result is rounded. */
  cpu = (int)((system_time_delta * 100 + time_delta / 2) / time_delta);

  last_system_time = system_time;
  last_time = sys_time;

  return cpu;
}

static inline uint64_t fast_strtou64_16(char ** endptr)
{
  unsigned char c;
  char * str = *endptr;
  uint64_t n = 0;

  while ((c = (unsigned char)*str++) != ' ')
  {
    c = ((c|0x20) - '0');
    if (c > 9)
      c = c - (('a' - '0') - 10);
    n = n*16 + c;
  }
  *endptr = str; /* We skip trailing space! */
  return n;
}

static inline char * skip_whitespace(const char * s)
{
#define isspace(c) ((' ' == (c)) || (((c) - 9) <= (13 - 9)))
  /* NB: isspace('\0') returns 0 */
  while (isspace(*s)) ++s;
  return (char *) s;
#undef isspace
}

static inline char * skip_fields(char * str, int count)
{
  do
  {
    while (*str++ != ' ')
      continue;
    /* we found a space char, str points after it */
  }
  while (--count);
  return str;
}

#ifdef __CYGWIN__
int get_pmemory_usage(pid_t pid, uint64_t * mem, uint64_t * vmem)
{
  char buf[PATH_MAX];
  int fd;
  int len;
  uint64_t mv[7];

  /* cygwin has not /proc/self/statm */
  len = snprintf(buf, PATH_MAX - 1, "/proc/%u/statm", pid);

  if (len <= 0)
    return -1;

  fd = open2_no_eintr(buf, O_RDONLY);

  if (-1 == fd)
    return -1;

  len = read_no_eintr(fd, buf, PATH_MAX);

  if (len <= 0)
  {
    close_no_eintr(fd);
    return -1;
  }

  if (7 != sscanf(buf, "%"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64"",
        &mv[0], &mv[1], &mv[2], &mv[3], &mv[4], &mv[5], &mv[6]))
  {
    close_no_eintr(fd);
    return -1;
  }

  if (mem) *mem = mv[1];

  if (vmem) *vmem = mv[3];

  close_no_eintr(fd);
  return 0;
}

#else/* __CYGWIN__ */

int get_pmemory_usage(pid_t pid, uint64_t * mem, uint64_t * vmem)
{
#define LINE_BUF_SIZE 1024
  char buf[PATH_MAX];
  int err = 0;
  int len;
  char line_buf[LINE_BUF_SIZE];
  FILE * file;

  uint64_t mapped_rw = 0;
  uint64_t mapped_ro = 0;

  len = snprintf(buf, PATH_MAX - 1, "/proc/%u/smaps", pid);
  if (len <= 0)
    return -1;

  file = fopen(buf, "r");
  if (NULL == file)
    return -1;

  while (fgets(line_buf, sizeof(line_buf), file))
  {
    char *tp;

#define SCAN(str) if (strncmp(line_buf, str, sizeof(str)-1) == 0) continue
    SCAN("Shared_Clean:");
    SCAN("Shared_Dirty:");
    SCAN("Private_Clean:");
    SCAN("Private_Dirty:");
#undef SCAN

    /* f7d29000-f7d39000 rw-s ADR M:m OFS FILE */
    tp = strchr(line_buf, '-');
    if (tp)
    {
      uint64_t sz;
      char w;
      *tp = ' ';
      tp = line_buf;
      sz = fast_strtou64_16(&tp); /* start */
      sz = (fast_strtou64_16(&tp) - sz); /* end - start */
      /* tp -> "rw-s" string */
      w = tp[1];
      /* skipping "rw-s ADR M:m OFS " */
      tp = skip_whitespace(skip_fields(tp, 4));
      /* filter out /dev/something (something != zero) */
      if (strncmp(tp, "/dev/", 5) != 0 || strcmp(tp, "/dev/zero\n") == 0)
      {
        if ('w' == w)
        {
          mapped_rw += sz;
        }
        else if ('-' == w)
        {
          mapped_ro += sz;
        }
      }
    }
  }

  if (mem) *mem = mapped_rw + mapped_ro;
  if (vmem) *vmem = 0;
  fclose(file);
  return err;
}
#endif/* __CYGWIN__ */

int get_memory_usage(uint64_t * mem, uint64_t * vmem)
{
  return get_pmemory_usage(getpid(), mem, vmem);
}

#else

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <psapi.h>
#include <assert.h>

#ifdef _MSC_VER
#pragma comment(lib, "psapi.lib")
#endif

static uint64_t file_time_to_utc(const FILETIME * ftime)
{
  LARGE_INTEGER li;

  assert(ftime);
  li.LowPart = ftime->dwLowDateTime;
  li.HighPart = ftime->dwHighDateTime;
  return li.QuadPart;
}

int get_processor_number()
{
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return (int)info.dwNumberOfProcessors;
}

int get_cpu_usage()
{
  static int processor_count = -1;
  static int64_t last_time = 0;
  static int64_t last_system_time = 0;


  FILETIME now;
  FILETIME creation_time;
  FILETIME exit_time;
  FILETIME kernel_time;
  FILETIME user_time;
  int64_t system_time;
  int64_t sys_time;
  int64_t system_time_delta;
  int64_t time_delta;

  int cpu = -1;


  if (-1 == processor_count)
  {
    processor_count = get_processor_number();
  }

  GetSystemTimeAsFileTime(&now);

  if (!GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time,
        &kernel_time, &user_time))
  {
    /*
     * We don't assert here because in some cases (such as in the Task Manager)
     * we may call this function on a process that has just exited but we have
     * not yet received the notification.
     */
    return -1;
  }

  system_time = (file_time_to_utc(&kernel_time) + file_time_to_utc(&user_time)) /
    processor_count;
  sys_time = file_time_to_utc(&now);

  if ((0 == last_system_time) || (0 == last_time))
  {
    /* First call, just set the last values. */
    last_system_time = system_time;
    last_time = sys_time;
    return 0;
  }

  system_time_delta = system_time - last_system_time;
  time_delta = sys_time - last_time;

  assert(time_delta != 0);

  if (0 == time_delta)
    return 0;

  /* We add time_delta / 2 so the result is rounded. */
  cpu = (int)((system_time_delta * 100 + time_delta / 2) / time_delta);
  last_system_time = system_time;
  last_time = sys_time;
  return cpu;
}

int get_memory_usage(uint64_t * mem, uint64_t * vmem)
{
  PROCESS_MEMORY_COUNTERS pmc;

  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
  {
    if (mem) *mem = pmc.WorkingSetSize;

    if (vmem) *vmem = pmc.PagefileUsage;

    return 0;
  }

  return -1;
}

#endif/* _WIN32 */

#if defined TEST
#include <stdio.h>

void dump_memory_usage(uint64_t mem)
{
  int gb = 0, mb = 0, kb = 0;

  kb = (int)(mem/1024);
  mb = (int)((mem/1024/1024) % 1024);
  gb = (int)(mem/1024/1024/1024);

  if (gb)
  {
    printf("%d.%.3d GB\n", gb, mb);
  }
  else if (mb)
  {
    printf("%d.%.3d MB\n", mb, kb);
  }
  else
  {
    printf("%d KB\n", kb);
  }
}

int main()
{
  int cpu;
  uint64_t mem, vmem;

  cpu = get_processor_number();
  get_memory_usage(&mem, &vmem);


  printf("CPU number=%d\n", cpu);
  dump_memory_usage(mem);
  dump_memory_usage(vmem);

  return 0;
}
#endif
