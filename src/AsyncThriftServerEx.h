/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_THRIFT_SERVER_H
#define ASYNC_THRIFT_SERVER_H

#include "AsyncThriftServerBase.h"
#include "AsyncProcessor.h"

namespace apache { namespace thrift { namespace async {

  /*
  * AsyncThriftServerEx(using TFramedTransport, TBinaryProtocol)
  *
  * socket layer asynchronous
  * RPC asynchronous
  */
  class AsyncThriftServerEx : public AsyncThriftServerBase
  {
  protected:
    AsyncThriftServerEx(
      const boost::shared_ptr<AsyncProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

  public:
    static boost::shared_ptr<AsyncThriftServerEx> create_server(
      const boost::shared_ptr<AsyncProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

    virtual void serve();

  protected:
    boost::shared_ptr<AsyncProcessor> processor_;
  };

} } } // namespace

#endif
