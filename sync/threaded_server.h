/** @file
 * @brief ThreadedServer that can run against avalanche
 * When the there are too many pending tasks, close the listening port.
 * Server may re-listen the port until too many pending tasks have gone.
 *
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef THREADED_SERVER_H
#define THREADED_SERVER_H

#include <base_server.h>
#include <set>
#include <concurrency/Thread.h>
#include <concurrency/Monitor.h>

//lint -esym(1712,ThreadedServer) default constructor not defined
//lint -esym(1732,ThreadedServer) no assignment operator
//lint -esym(1733,ThreadedServer) no copy constructoro

namespace apache { namespace thrift { namespace sync {

  using ::apache::thrift::concurrency::Monitor;
  using ::apache::thrift::concurrency::ThreadFactory;

  class ThreadedServer : public BaseServer
  {
    public:
      class Task;
      friend class Task;

      ThreadedServer(boost::shared_ptr<TProcessor> processor,
          boost::shared_ptr<TServerTransport> serverTransport,
          boost::shared_ptr<TTransportFactory> transportFactory,
          boost::shared_ptr<TProtocolFactory> protocolFactory,
          size_t workerThreadCountMax = 1000);

      virtual ~ThreadedServer();

      virtual void serve();

    protected:
      boost::shared_ptr<ThreadFactory> threadFactory_;
      Monitor tasksMonitor_;
      std::set<Task*> tasks_;
      const size_t workerThreadCountMax_;
  };

} } }

#endif
