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

namespace apache { namespace thrift { namespace async {

  /*
  * AsyncThriftServer(using TFramedTransport, TBinaryProtocol)
  *
  * socket layer asynchronous
  * RPC synchronous
  */
  class AsyncThriftServer : public AsyncThriftServerBase
  {
  protected:
    AsyncThriftServer(
      const boost::shared_ptr<TProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

  public:
    static boost::shared_ptr<AsyncThriftServer> create_server(
      const boost::shared_ptr<TProcessor>& processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      size_t max_client);

    virtual void serve();

  protected:
    boost::shared_ptr<TProcessor> processor_;
  };

} } } // namespace

#endif
