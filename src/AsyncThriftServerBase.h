/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_THRIFT_SERVER_BASE_H
#define ASYNC_THRIFT_SERVER_BASE_H

#include <set>
#include "AsyncConnection.h"

namespace apache { namespace thrift { namespace async {

  class AsyncServerConnection;
  class AsyncServerConnectionFactory;
  class AsyncThriftServerBase;


  class AsyncServerConnection : public AsyncConnection
  {
  protected:
    boost::weak_ptr<AsyncThriftServerBase> parent_;

  protected:
    AsyncServerConnection(boost::asio::io_service& io_service,
      const boost::shared_ptr<AsyncThriftServerBase>& parent);
  public:
    virtual ~AsyncServerConnection();
  protected:
    virtual void on_close(const boost::system::error_code * ec);
    virtual void on_handle_frame();
  };


  class AsyncServerConnectionFactory
  {
  protected:
    boost::asio::io_service& io_service_;
    boost::shared_ptr<AsyncThriftServerBase> server_;

  protected:
    AsyncServerConnectionFactory(boost::asio::io_service& io_service,
      const boost::shared_ptr<AsyncThriftServerBase>& server);
  public:
    virtual ~AsyncServerConnectionFactory();
    virtual boost::shared_ptr<AsyncConnection> create() = 0;
  };


  /*
  * AsyncThriftServerBase(using TFramedTransport, TBinaryProtocol)
  */
  class AsyncThriftServerBase : public TServer,
    public boost::enable_shared_from_this<AsyncThriftServerBase>,
    private boost::noncopyable
  {
  protected:
    //acceptor must be opened and listening
    AsyncThriftServerBase(
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
      size_t thread_pool_size,
      //0 means no maximum client limit
      //the limit will take effect to the new coming clients
      size_t max_client);

  public:
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
    struct ConnectionSPLess
    {
      bool operator()(const ConnectionSP& a, const ConnectionSP& b)const
      {
        return a.get() < b.get();
      }
    };
    typedef std::set<ConnectionSP, ConnectionSPLess> ConnectionSPSet;

  public:
    void remove_client(const ConnectionSP& conn);
  protected:
    void async_accept();
    void handle_accept(ConnectionSP conn, const boost::system::error_code& ec);

    boost::shared_ptr<AsyncServerConnectionFactory> conn_factory_;
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    boost::asio::io_service& io_service_;
    const size_t thread_pool_size_;
    const size_t max_client_;

    ConnectionSP new_connection_;

    boost::mutex client_mutex_;
    ConnectionSPSet clients_;
  };

} } } // namespace

#endif
