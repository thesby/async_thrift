/** @file
* @brief semi-asynchronous thrift server(using TProcessor)
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <AsyncThriftServer.h>
#include <AsyncException.h>

namespace apache { namespace thrift { namespace async {

  namespace
  {
    class Connection : public AsyncConnection
    {
    private:
      boost::shared_ptr<TProcessor> processor_;

    public:
      Connection(boost::asio::io_service& io_service,
        const boost::shared_ptr<TProcessor>& processor)
        :AsyncConnection(io_service), processor_(processor)
      {
      }

    protected:
      virtual void on_handle_frame()
      {
        try
        {
          processor_->process(input_proto_, output_proto_);//may throw
          //NOTICE!!!
          //AsyncThriftServer does not support oneway functions, it always replies here.
          //Because we could not know whether a RPC is oneway.
          //This defect will not be fixed, AsyncThriftServerEx is a substitute.
          start_write_output_buffer();
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
    };
  }

  /************************************************************************/
  AsyncThriftServer_SingleIOService::AsyncThriftServer_SingleIOService(
    const boost::shared_ptr<TProcessor>& processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
    :AsyncThriftServerBase(acceptor, thread_pool_size, max_client),
    processor_(processor)
  {
  }

  boost::shared_ptr<AsyncThriftServerBase> AsyncThriftServer_SingleIOService::create_server(
    const boost::shared_ptr<TProcessor>& processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
  {
    return boost::shared_ptr<AsyncThriftServer_SingleIOService>(
      new AsyncThriftServer_SingleIOService(processor, acceptor, thread_pool_size, max_client));
  }

  AsyncThriftServer_SingleIOService::ConnectionSP
    AsyncThriftServer_SingleIOService::create_connection()
  {
    return ConnectionSP(new Connection(get_io_service(), processor_));
  }

  /************************************************************************/
  AsyncThriftServer_IOServicePerThread::AsyncThriftServer_IOServicePerThread(
    const boost::shared_ptr<TProcessor>& processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
    :AsyncThriftServerBase(acceptor, thread_pool_size, max_client),
    processor_(processor),
    io_service_pool_(thread_pool_size)
  {
  }

  boost::shared_ptr<AsyncThriftServerBase> AsyncThriftServer_IOServicePerThread::create_server(
    const boost::shared_ptr<TProcessor>& processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
  {
    return boost::shared_ptr<AsyncThriftServer_IOServicePerThread>(
      new AsyncThriftServer_IOServicePerThread(processor, acceptor, thread_pool_size, max_client));
  }

  void AsyncThriftServer_IOServicePerThread::serve()
  {
    get_io_service().reset();
    async_accept();
    boost::thread_group tg;
    tg.create_thread(boost::bind(&boost::asio::io_service::run, &get_io_service()));

    io_service_pool_.run();

    tg.join_all();
  }

  void AsyncThriftServer_IOServicePerThread::stop()
  {
    get_io_service().stop();
    io_service_pool_.stop();
  }

  AsyncThriftServer_IOServicePerThread::ConnectionSP
    AsyncThriftServer_IOServicePerThread::create_connection()
  {
    return ConnectionSP(new Connection(io_service_pool_.get_io_service(), processor_));
  }

} } } // namespace
