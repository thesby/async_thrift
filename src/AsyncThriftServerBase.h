/** @file
* @brief base class for asynchronous thrift server
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_THRIFT_SERVER_BASE_H
#define ASYNC_THRIFT_SERVER_BASE_H

#include "AsyncConnection.h"

namespace apache { namespace thrift { namespace async {

  /*
  * AsyncThriftServerBase(using TFramedTransport, TBinaryProtocol)
  */
  class AsyncThriftServerBase : public TServer, private boost::noncopyable
  {
  private:
    boost::asio::io_service& io_service_;
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    const size_t thread_pool_size_;
    const size_t max_client_;

  public:
    //acceptor must be opened and listening
    AsyncThriftServerBase(
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      //max_client takes no effect now
      size_t max_client);
    virtual ~AsyncThriftServerBase();

    boost::asio::io_service& get_io_service()
    {
      return io_service_;
    }

    size_t get_thread_pool_size()const
    {
      return thread_pool_size_;
    }

    size_t get_max_client()const
    {
      return max_client_;
    }

    //from TServer
    virtual void serve();
    virtual void stop();

  protected:
    typedef boost::shared_ptr<AsyncConnection> ConnectionSP;
    virtual ConnectionSP create_connection() = 0;
    void async_accept();
  private:
    void handle_accept(ConnectionSP conn, const boost::system::error_code& ec);
  };

} } } // namespace

#endif
