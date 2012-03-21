/** @file
* @brief asynchronous thrift server(asynchronous IO/asynchronous processor:AsyncProcessor)
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_THRIFT_SERVER_EX_H
#define ASYNC_THRIFT_SERVER_EX_H

#include <AsyncThriftServerBase.h>
#include <AsyncProcessor.h>
#include <io_service_pool.h>

namespace apache { namespace thrift { namespace async {

  /*
  * AsyncThriftServerEx_SingleIOService(using TFramedTransport, TBinaryProtocol)
  *
  * socket layer asynchronous
  * RPC asynchronous
  *
  * a single io_service and a thread pool calling io_service::run()
  */
  class AsyncThriftServerEx_SingleIOService : public AsyncThriftServerBase
  {
  protected:
    boost::shared_ptr<AsyncProcessor> processor_;

  public:
    AsyncThriftServerEx_SingleIOService(
      const boost::shared_ptr<AsyncProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

    static boost::shared_ptr<AsyncThriftServerBase> create_server(
      const boost::shared_ptr<AsyncProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

  protected:
    virtual ConnectionSP create_connection();
  };

  /************************************************************************/
  /*
  * AsyncThriftServerEx_IOServicePerThread(using TFramedTransport, TBinaryProtocol)
  *
  * socket layer asynchronous
  * RPC asynchronous
  *
  * an io_service-per-thread
  */
  class AsyncThriftServerEx_IOServicePerThread : public AsyncThriftServerBase
  {
  protected:
    boost::shared_ptr<AsyncProcessor> processor_;
    io_service_pool io_service_pool_;

  public:
    AsyncThriftServerEx_IOServicePerThread(
      const boost::shared_ptr<AsyncProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

    static boost::shared_ptr<AsyncThriftServerBase> create_server(
      const boost::shared_ptr<AsyncProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

    virtual void serve();
    virtual void stop();
  protected:
    virtual ConnectionSP create_connection();
  };

  typedef AsyncThriftServerEx_IOServicePerThread AsyncThriftServerEx;

} } } // namespace

#endif
