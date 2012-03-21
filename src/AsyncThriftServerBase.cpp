/** @file
* @brief base class for asynchronous thrift server
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <AsyncThriftServerBase.h>
#include <io_service_pool.h>

namespace apache { namespace thrift { namespace async {

  AsyncThriftServerBase::AsyncThriftServerBase(
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor>& acceptor,
    size_t thread_pool_size,
    size_t max_client)
    :TServer(boost::shared_ptr<TProcessor>()),
    io_service_(acceptor->get_io_service()),
    acceptor_(acceptor),
    thread_pool_size_(thread_pool_size?thread_pool_size:1),
    max_client_(max_client)
  {
  }

  AsyncThriftServerBase::~AsyncThriftServerBase()
  {
    AsyncThriftServerBase::stop();
  }

  void AsyncThriftServerBase::serve()
  {
    get_io_service().reset();
    async_accept();
    boost::thread_group tg;
    for (size_t i=0; i<get_thread_pool_size(); i++)
    {
      tg.create_thread(boost::bind(run_io_service_tss, &get_io_service()));
    }
    tg.join_all();
  }

  void AsyncThriftServerBase::stop()
  {
    get_io_service().stop();
  }

  void AsyncThriftServerBase::async_accept()
  {
    ConnectionSP new_conn = create_connection();
    acceptor_->async_accept(new_conn->get_socket(),
      boost::bind(&AsyncThriftServerBase::handle_accept, this, new_conn, _1));
  }

  void AsyncThriftServerBase::handle_accept(ConnectionSP conn, const boost::system::error_code& ec)
  {
    if (!ec)
    {
      conn->start_recv(true);
      async_accept();
    }
  }

} } } // namespace
