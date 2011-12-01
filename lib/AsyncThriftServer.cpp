/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <assert.h>
#include <boost/bind.hpp>
#include <boost/weak_ptr.hpp>
#include <Thrift.h>
#include <AsyncConnection.h>
#include <AsyncThriftServer.h>

namespace apache { namespace thrift { namespace async {

  using ::apache::thrift::GlobalOutput;

  /************************************************************************/
  class AsyncThriftServer::Connection : public AsyncConnection
  {
  private:
    boost::weak_ptr<AsyncThriftServer> parent_;
    boost::shared_ptr<apache::thrift::TProcessor> processor_;

  public:
    Connection(boost::asio::io_service& io_service,
      boost::shared_ptr<AsyncThriftServer> parent,
      boost::shared_ptr< ::apache::thrift::TProcessor> processor)
      :AsyncConnection(io_service), parent_(parent), processor_(processor)
    {
    }

    virtual ~Connection()
    {
    }

  protected:
    virtual void on_close(const boost::system::error_code * ec)
    {
      boost::shared_ptr<AsyncThriftServer> parent = parent_.lock();
      if (parent)
        parent->remove_client(
        boost::dynamic_pointer_cast<AsyncThriftServer::Connection, AsyncConnection>(shared_from_this()));
    }

    virtual void on_handle_frame()
    {
      try
      {
        processor_->process(input_proto_, output_proto_);
      }
      catch (...)
      {
        GlobalOutput.printf("caught an exception in Processor::process");
        close();
        return;
      }

      uint32_t out_frame_size;
      uint8_t * out_frame;
      output_buffer_->getBuffer(&out_frame, &out_frame_size);

      boost::asio::async_write(*socket_,
        boost::asio::buffer(out_frame, out_frame_size),
        boost::asio::transfer_all(),//transfer_all
        boost::bind(&AsyncThriftServer::Connection::handle_write, shared_from_this(), _1, _2));
    }
  };

  /************************************************************************/
  AsyncThriftServer::AsyncThriftServer(
    boost::shared_ptr< ::apache::thrift::TProcessor> processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor,
    size_t thread_pool_size,
    size_t max_client)
    :TServer(processor), acceptor_(acceptor),
    thread_pool_size_(thread_pool_size?thread_pool_size:1),
    max_client_(max_client),
    io_service_(acceptor->get_io_service())
  {
    assert(acceptor);
  }

  AsyncThriftServer::~AsyncThriftServer()
  {
    stop();//NOTICE: no polymorphisms here

    //close server socket
    boost::system::error_code ec;
    acceptor_->close(ec);

    new_connection_.reset();

    {
      boost::mutex::scoped_lock guard(client_mutex_);
      client_.clear();
    }
  }

  void AsyncThriftServer::serve()
  {
    io_service_.reset();

    async_accept();

    boost::thread_group tg;
    for (size_t i=0; i<thread_pool_size_; i++)
      tg.create_thread(boost::bind(&boost::asio::io_service::run, &io_service_));

    tg.join_all();
  }

  void AsyncThriftServer::stop()
  {
    io_service_.stop();
  }

  void AsyncThriftServer::async_accept()
  {
    new_connection_.reset(new Connection(io_service_, shared_from_this(), getProcessor()));

    //pass "new_connection_" to a bound handler that could hold a copy of "new_connection_",
    //which makes it still alive
    acceptor_->async_accept(new_connection_->get_socket(),
      boost::bind(&AsyncThriftServer::handle_accept, this, new_connection_, _1));
  }

  void AsyncThriftServer::handle_accept(ConnectionSP conn,
    const boost::system::error_code& ec)
  {
    if (!ec)
    {
      bool accept_again = true;
      {
        boost::mutex::scoped_lock guard(client_mutex_);
        client_.insert(conn);

        if (max_client_ && client_.size() >= max_client_)
          accept_again = false;
      }

      if (accept_again)
        async_accept();
      conn->start_recv(true);
    }
  }

  void AsyncThriftServer::remove_client(const ConnectionSP& conn)
  {
    boost::mutex::scoped_lock guard(client_mutex_);
    client_.erase(conn);

    if (max_client_ && client_.size() < max_client_)
      async_accept();
  }

} } } // namespace
