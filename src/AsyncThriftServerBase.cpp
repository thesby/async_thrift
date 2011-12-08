/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <AsyncThriftServerBase.h>

namespace apache { namespace thrift { namespace async {

  AsyncServerConnection::AsyncServerConnection(boost::asio::io_service& io_service,
    const boost::shared_ptr<AsyncThriftServerBase>& parent)
    :AsyncConnection(io_service), parent_(parent)
  {
  }

  AsyncServerConnection::~AsyncServerConnection()
  {
  }

  void AsyncServerConnection::on_close(const boost::system::error_code * ec)
  {
    boost::shared_ptr<AsyncThriftServerBase> parent = parent_.lock();
    if (parent)
      parent->remove_client(shared_from_this());
  }

  void AsyncServerConnection::on_handle_frame()
  {
    //overrides
  }

  /************************************************************************/
  AsyncServerConnectionFactory::AsyncServerConnectionFactory(boost::asio::io_service& io_service,
    const boost::shared_ptr<AsyncThriftServerBase>& server)
    :io_service_(io_service), server_(server)
  {
  }

  AsyncServerConnectionFactory::~AsyncServerConnectionFactory()
  {
  }

  /************************************************************************/
  AsyncThriftServerBase::AsyncThriftServerBase(
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
    :TServer(boost::shared_ptr<TProcessor>()),
    acceptor_(acceptor),
    io_service_(acceptor->get_io_service()),
    thread_pool_size_(thread_pool_size?thread_pool_size:1),
    max_client_(max_client)
  {
  }

  AsyncThriftServerBase::~AsyncThriftServerBase()
  {
    AsyncThriftServerBase::stop();//NOTICE: no polymorphisms here

    //close server socket
    boost::system::error_code ec;
    acceptor_->close(ec);

    new_connection_.reset();

    {
      boost::mutex::scoped_lock guard(client_mutex_);
      clients_.clear();
    }
  }

  void AsyncThriftServerBase::serve()
  {
    io_service_.reset();

    async_accept();

    boost::thread_group tg;
    for (size_t i=0; i<thread_pool_size_; i++)
      tg.create_thread(boost::bind(&boost::asio::io_service::run, &io_service_));

    tg.join_all();
  }

  void AsyncThriftServerBase::stop()
  {
    io_service_.stop();
  }

  void AsyncThriftServerBase::async_accept()
  {
    new_connection_ = conn_factory_->create();

    //pass "new_connection_" to a bound handler that could hold a copy of "new_connection_",
    //which makes it still alive
    acceptor_->async_accept(new_connection_->get_socket(),
      boost::bind(&AsyncThriftServerBase::handle_accept, this, new_connection_, _1));
  }

  void AsyncThriftServerBase::handle_accept(ConnectionSP conn,
    const boost::system::error_code& ec)
  {
    if (!ec)
    {
      bool accept_again = true;
      {
        boost::mutex::scoped_lock guard(client_mutex_);
        clients_.insert(conn);

        if (max_client_ && clients_.size() >= max_client_)
          accept_again = false;
      }

      if (accept_again)
        async_accept();
      conn->start_recv(true);
    }
  }

  void AsyncThriftServerBase::remove_client(const ConnectionSP& conn)
  {
    boost::mutex::scoped_lock guard(client_mutex_);
    clients_.erase(conn);

    if (max_client_ && clients_.size() < max_client_)
      async_accept();
  }

} } } // namespace
