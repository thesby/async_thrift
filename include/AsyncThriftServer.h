/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_THRIFT_SERVER_H
#define ASYNC_THRIFT_SERVER_H

#include <stdint.h>
#include <vector>
#include <string>
#include <set>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread.hpp>
#include <server/TServer.h>
#include <TProcessor.h>

namespace apache { namespace thrift { namespace async {

  class AsyncThriftServer : public ::apache::thrift::server::TServer,
    public boost::enable_shared_from_this<AsyncThriftServer>,
    private boost::noncopyable
  {
  protected:
    //socket must be opened and listening
    AsyncThriftServer(
      boost::shared_ptr< ::apache::thrift::TProcessor> processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor,
      size_t thread_pool_size,
      //0 means no maximum client limit
      //the limit will take effect to the new coming clients
      size_t max_client);

  public:
    static boost::shared_ptr<AsyncThriftServer> create_server(
      boost::shared_ptr< ::apache::thrift::TProcessor> processor,
      const boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor,
      size_t thread_pool_size,
      size_t max_client = 0);

    virtual ~AsyncThriftServer();

    boost::asio::io_service& get_io_service()
    {
      return io_service_;
    }

    const boost::asio::io_service& get_io_service()const
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
    class Connection;//client connection
    friend class Connection;
    typedef boost::shared_ptr<Connection> ConnectionSP;
    struct ConnectionSPLess
    {
      bool operator()(const ConnectionSP& a, const ConnectionSP& b)const
      {
        return a.get() < b.get();
      }
    };
    typedef std::set<ConnectionSP, ConnectionSPLess> ConnectionSPSet;

    void async_accept();
    void handle_accept(ConnectionSP conn, const boost::system::error_code& ec);
    void remove_client(const ConnectionSP& conn);

    boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    const size_t thread_pool_size_;
    const size_t max_client_;
    boost::asio::io_service& io_service_;

    ConnectionSP new_connection_;
    boost::mutex client_mutex_;
    ConnectionSPSet client_;
  };

} } } // namespace

#endif
