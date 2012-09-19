/** @file
 * @brief asynchronous thrift servers
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef ASYNC_SERVER_H
#define ASYNC_SERVER_H

#include <async_processor.h>
#include <io_service_pool.h>

//lint -esym(1712,AsyncThriftServer) default constructor not defined
//lint -esym(1732,AsyncThriftServer) no assignment operator
//lint -esym(1733,AsyncThriftServer) no copy constructor

namespace apache { namespace thrift { namespace async {

  /*
   * AsyncThriftServer(using TFramedTransport, TBinaryProtocol)
   *
   * socket layer asynchronous
   * RPC synchronous/asynchronous
   *
   * an io_service-per-thread
   */
  class AsyncThriftServer : public TServer, private boost::noncopyable
  {
    private:
      class Impl;
      Impl * impl_;

    public:
      AsyncThriftServer(
          const boost::shared_ptr<TProcessor>& processor,
          const boost::asio::ip::tcp::endpoint& endpoint,
          IOServicePool& pool);

      AsyncThriftServer(
          const boost::shared_ptr<AsyncProcessor>& async_processor,
          const boost::asio::ip::tcp::endpoint& endpoint,
          IOServicePool& pool);

      virtual ~AsyncThriftServer();

      virtual void serve();// may throw
      virtual void stop();

      IOServicePool& get_io_service_pool();
  };

} } } // namespace

#endif
