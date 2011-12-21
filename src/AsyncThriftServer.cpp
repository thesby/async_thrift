/** @file
* @brief semi-asynchronous thrift server(using TProcessor)
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <AsyncThriftServer.h>

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
          //NOTICE:it does not support oneway functions, because it always replies
          start_write_output_buffer();
        }
        catch (std::exception& e)
        {
          GlobalOutput.printf("caught an exception in TProcessor::process: %s", e.what());
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

  AsyncThriftServer_SingleIOService::ConnectionSP AsyncThriftServer_SingleIOService::create_connection()
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

  AsyncThriftServer_IOServicePerThread::ConnectionSP AsyncThriftServer_IOServicePerThread::create_connection()
  {
    return ConnectionSP(new Connection(io_service_pool_.get_io_service(), processor_));
  }

} } } // namespace
