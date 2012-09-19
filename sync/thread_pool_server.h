/** @file
 * @brief ThreadPoolServer that can run against avalanche
 * When the there are too many pending tasks, close the listening port.
 * Server may re-listen the port until too many pending tasks have gone.
 *
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef THREAD_POOL_SERVER_H
#define THREAD_POOL_SERVER_H

#include <base_server.h>
#include <concurrency/ThreadManager.h>

namespace apache { namespace thrift { namespace sync {

  using ::apache::thrift::concurrency::ThreadManager;

  class ThreadPoolServer : public BaseServer
  {
    public:
      class Task;
      friend class Task;

      ThreadPoolServer(boost::shared_ptr<TProcessor> processor,
          boost::shared_ptr<TServerTransport> serverTransport,
          boost::shared_ptr<TTransportFactory> transportFactory,
          boost::shared_ptr<TProtocolFactory> protocolFactory,
          int64_t addTaskTimeout = 0,
          int64_t addTaskExpiration = 100,
          size_t workerThreadCount = 1000,
          size_t pendingTaskCountMax = 100);

      virtual ~ThreadPoolServer();

      virtual void serve();

    protected:
      boost::shared_ptr<ThreadManager> threadManager_;

      // refer ThreadManager::add(task, timeout, expiration);
      // Time to wait in milliseconds to add a task '''when pendingTaskCountMax_ is not zero''':
      // >0 : Time to wait in milliseconds
      // 0  : Wait forever to threadManager_
      // -1 : Return immediately if the number of pending task exceeds pendingTaskCountMax_
      const int64_t addTaskTimeout_;
      // '''When nonzero''', the number of milliseconds the task is valid to be run;
      // if exceeded, the task will be dropped off threadManager_ and not run.
      const int64_t addTaskExpiration_;

      // refer ThreadManager::newSimpleThreadManager(count, pendingTaskCountMax);
      // internal thread number of workers in threadManager_
      const size_t workerThreadCount_;
      // '''When nonzero''', the max number of pending task.
      const size_t pendingTaskCountMax_;

  };

} } }

#endif
