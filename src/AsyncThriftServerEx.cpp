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
#include <AsyncThriftServerEx.h>

namespace apache { namespace thrift { namespace async {

  class Connection : public AsyncServerConnection
  {
  protected:
    boost::shared_ptr<AsyncProcessor> processor_;

  public:
    Connection(boost::asio::io_service& io_service,
      const boost::shared_ptr<AsyncThriftServerBase>& parent,
      const boost::shared_ptr<AsyncProcessor>& processor)
      :AsyncServerConnection(io_service, parent), processor_(processor)
    {
    }

  protected:
    virtual void on_handle_frame()
    {
      try
      {
        processor_->process(
          boost::bind(&Connection::callback,
          boost::dynamic_pointer_cast<Connection, AsyncConnection>(shared_from_this()), _1),
          input_proto_, output_proto_);
      }
      catch (...)
      {
        GlobalOutput.printf("caught an exception in Processor::process");
        close();
        return;
      }
    }

    void callback(const boost::system::error_code& ec)
    {
      if (ec)
      {
        on_close(&ec);
        return;
      }

      if (!is_open())
      {
        close();
        return;
      }

      start_write_output_buffer();
    }
  };

  /************************************************************************/
  class ConnectionFactory : public AsyncServerConnectionFactory
  {
  protected:
    boost::shared_ptr<AsyncProcessor> processor_;

  public:
    ConnectionFactory(boost::asio::io_service& io_service,
      const boost::shared_ptr<AsyncThriftServerBase>& server,
      const boost::shared_ptr<AsyncProcessor>& processor)
      :AsyncServerConnectionFactory(io_service, server), processor_(processor)
    {
    }

    virtual ~ConnectionFactory()
    {
    }

    virtual boost::shared_ptr<AsyncConnection> create()
    {
      return boost::shared_ptr<AsyncConnection>(
        new Connection(io_service_, server_, processor_));
    }
  };

  /************************************************************************/
  AsyncThriftServerEx::AsyncThriftServerEx(
    const boost::shared_ptr<AsyncProcessor>& processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
    :AsyncThriftServerBase(acceptor, thread_pool_size, max_client),
    processor_(processor)
  {
  }

  boost::shared_ptr<AsyncThriftServerEx> AsyncThriftServerEx::create_server(
    const boost::shared_ptr<AsyncProcessor>& processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
  {
    return boost::shared_ptr<AsyncThriftServerEx>(
      new AsyncThriftServerEx(processor, acceptor, thread_pool_size, max_client));
  }

  void AsyncThriftServerEx::serve()
  {
    conn_factory_.reset(new ConnectionFactory(get_io_service(), shared_from_this(), processor_));
    AsyncThriftServerBase::serve();
  }

} } } // namespace
