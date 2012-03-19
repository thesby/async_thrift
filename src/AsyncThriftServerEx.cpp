/** @file
* @brief full asynchronous thrift server(using AsyncProcessor)
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <AsyncThriftServerEx.h>
#include <AsyncException.h>

namespace apache { namespace thrift { namespace async {

  namespace
  {
    class Connection : public AsyncConnection
    {
    private:
      boost::shared_ptr<AsyncProcessor> processor_;

    public:
      Connection(boost::asio::io_service& io_service,
        const boost::shared_ptr<AsyncProcessor>& processor)
        :AsyncConnection(io_service), processor_(processor)
      {
      }

    protected:
      virtual void on_handle_frame()
      {
        try
        {
          processor_->process(
            input_proto_, output_proto_,
            boost::bind(&Connection::async_process, shared_from_this(), _1, _2));//may throw
        }
        catch (TApplicationException& e)
        {
          GlobalOutput.printf("on_handle_frame: %s", e.what());
          boost::system::error_code ec = make_error_code(e);
          on_close(&ec);
        }
        catch (TProtocolException& e)
        {
          GlobalOutput.printf("on_handle_frame: %s", e.what());
          boost::system::error_code ec = make_error_code(e);
          on_close(&ec);
        }
        catch (TTransportException& e)
        {
          GlobalOutput.printf("on_handle_frame: %s", e.what());
          boost::system::error_code ec = make_error_code(e);
          on_close(&ec);
        }
        catch (TException& e)
        {
          GlobalOutput.printf("on_handle_frame: %s", e.what());
          boost::system::error_code ec = make_error_code(e);
          on_close(&ec);
        }
        catch (...)
        {
          GlobalOutput.printf("on_handle_frame: error");
          boost::system::error_code ec(
            boost::system::posix_error::bad_message, boost::system::get_posix_category());
          on_close(&ec);
        }
      }

      void on_async_process(const boost::system::error_code& ec, bool is_oneway)
      {
        if (ec || !is_open())
          return;

        if (!is_oneway)
          start_write_output_buffer();
        else
          start_recv(false);
      }
    };
  }

  /************************************************************************/
  AsyncThriftServerEx_SingleIOService::AsyncThriftServerEx_SingleIOService(
    const boost::shared_ptr<AsyncProcessor>& processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
    :AsyncThriftServerBase(acceptor, thread_pool_size, max_client),
    processor_(processor)
  {
  }

  boost::shared_ptr<AsyncThriftServerBase> AsyncThriftServerEx_SingleIOService::create_server(
    const boost::shared_ptr<AsyncProcessor>& processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
  {
    return boost::shared_ptr<AsyncThriftServerEx_SingleIOService>(
      new AsyncThriftServerEx_SingleIOService(processor, acceptor, thread_pool_size, max_client));
  }

  AsyncThriftServerEx_SingleIOService::ConnectionSP
    AsyncThriftServerEx_SingleIOService::create_connection()
  {
    return ConnectionSP(new Connection(get_io_service(), processor_));
  }

  /************************************************************************/
  AsyncThriftServerEx_IOServicePerThread::AsyncThriftServerEx_IOServicePerThread(
    const boost::shared_ptr<AsyncProcessor>& processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
    :AsyncThriftServerBase(acceptor, thread_pool_size, max_client),
    processor_(processor),
    io_service_pool_(thread_pool_size)
  {
  }

  boost::shared_ptr<AsyncThriftServerBase> AsyncThriftServerEx_IOServicePerThread::create_server(
    const boost::shared_ptr<AsyncProcessor>& processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
  {
    return boost::shared_ptr<AsyncThriftServerEx_IOServicePerThread>(
      new AsyncThriftServerEx_IOServicePerThread(
        processor, acceptor, thread_pool_size, max_client));
  }

  void AsyncThriftServerEx_IOServicePerThread::serve()
  {
    get_io_service().reset();
    async_accept();
    boost::thread_group tg;
    tg.create_thread(boost::bind(&boost::asio::io_service::run, &get_io_service()));

    io_service_pool_.run();

    tg.join_all();
  }

  void AsyncThriftServerEx_IOServicePerThread::stop()
  {
    get_io_service().stop();
    io_service_pool_.stop();
  }

  AsyncThriftServerEx_IOServicePerThread::ConnectionSP
    AsyncThriftServerEx_IOServicePerThread::create_connection()
  {
    return ConnectionSP(new Connection(io_service_pool_.get_io_service(), processor_));
  }

} } } // namespace
