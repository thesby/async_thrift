/** @file
* @brief semi-asynchronous thrift server(aasynchronous IO/synchronous processor:TProcessor)
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_THRIFT_SERVER_H
#define ASYNC_THRIFT_SERVER_H

#include <AsyncThriftServerBase.h>
#include <io_service_pool.h>

namespace apache { namespace thrift { namespace async {

  /*
  * AsyncThriftServer_SingleIOService(using TFramedTransport, TBinaryProtocol)
  *
  * socket layer asynchronous
  * RPC synchronous
  *
  * a single io_service and a thread pool calling io_service::run()
  */
  class AsyncThriftServer_SingleIOService : public AsyncThriftServerBase
  {
  protected:
    boost::shared_ptr<TProcessor> processor_;

  public:
    AsyncThriftServer_SingleIOService(
      const boost::shared_ptr<TProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

    static boost::shared_ptr<AsyncThriftServerBase> create_server(
      const boost::shared_ptr<TProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

  protected:
    virtual ConnectionSP create_connection();
  };

  /************************************************************************/
  /*
  * AsyncThriftServer_IOServicePerThread(using TFramedTransport, TBinaryProtocol)
  *
  * socket layer asynchronous
  * RPC synchronous
  *
  * an io_service-per-thread
  */
  class AsyncThriftServer_IOServicePerThread : public AsyncThriftServerBase
  {
  protected:
    boost::shared_ptr<TProcessor> processor_;
    IOServicePool io_service_pool_;

  public:
    AsyncThriftServer_IOServicePerThread(
      const boost::shared_ptr<TProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

    static boost::shared_ptr<AsyncThriftServerBase> create_server(
      const boost::shared_ptr<TProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

    virtual void serve();
    virtual void stop();
  protected:
    virtual ConnectionSP create_connection();
  };

  typedef AsyncThriftServer_IOServicePerThread AsyncThriftServer;

} } } // namespace

#endif
