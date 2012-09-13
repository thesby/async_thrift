/** @file
 * @brief process statistics
 * @author zhangyafeikimi@gmail.com
 * @date
 * @version
 *
 */
#ifndef PROCESS_STAT_H
#define PROCESS_STAT_H

#ifdef _MSC_VER
typedef long long int64_t;
typedef unsigned long long uint64_t;
#else
# include <stdint.h>
#endif
#ifndef _WIN32
# include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

  /*
   * return the number of CPU
   */
  int get_processor_number();

  /*
   * return the CPU usage from previous call of get_cpu_usage to current call of get_cpu_usage
   * return value is in range [0, 100]
   * return -1 failure
   */
  int get_cpu_usage();

  /*
   * get the process' memory/virtual memory usage
   * return 0 OK/-1 failure
   */
  int get_memory_usage(uint64_t * mem, uint64_t * vmem);
#ifndef _WIN32
  int get_pmemory_usage(pid_t pid, uint64_t * mem, uint64_t * vmem);
#endif

#ifdef __cplusplus
}
#endif

#endif/* PROCESS_STAT_H */
