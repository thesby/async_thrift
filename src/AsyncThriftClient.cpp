/** @file
* @brief base class for asynchronous thrift client
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <AsyncThriftClient.h>

namespace apache { namespace thrift { namespace async {

  AsyncThriftClient::AsyncThriftClient()
    :AsyncConnection()
  {
  }

  AsyncThriftClient::AsyncThriftClient(
    const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket)
    :AsyncConnection(socket)
  {
  }

  AsyncThriftClient::~AsyncThriftClient()
  {
    AsyncThriftClient::on_close(0);
  }

  void AsyncThriftClient::on_close(const boost::system::error_code * ec)
  {
    if (!socket_)
      return;

    io_service_ = 0;
    boost::system::error_code _ec;
    socket_->close(_ec);//close it manually
    socket_.reset();
    strand_.reset();

    //got error_code
    if (!async_op_list_.empty())
    {
      boost::system::error_code real_ec;
      if (ec == 0)
      {
        real_ec.assign(boost::system::posix_error::operation_canceled,
          boost::system::get_posix_category());
        ec = &real_ec;
      }
    }

    //execute the pending and queued callback if they exist
    pending_async_op_.reset();
    boost::shared_ptr<AsyncOp> op;
    while (!async_op_list_.empty())
    {
      op = async_op_list_.front();
      async_op_list_.pop_front();
      assert(op);
      op->callback(*ec);
    }
  }

  void AsyncThriftClient::on_attach(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket)
  {
    if (!async_op_list_.empty())
      throw TTransportException("can not attach sockets that has pending asynchronous operations");

    AsyncConnection::on_attach(socket);
  }

  void AsyncThriftClient::on_detach()
  {
    if (!async_op_list_.empty())
      throw TTransportException("can not detach sockets that has pending asynchronous operations");

    AsyncConnection::on_detach();
  }

  void AsyncThriftClient::on_handle_read(
    const boost::system::error_code& ec, size_t bytes_transferred)
  {
    if (!pending_async_op_
      || async_op_list_.empty()
      || pending_async_op_.get() != async_op_list_.front().get())
      //operation has been already canceled
      return;

    if (ec)
    {
      on_close(&ec);
      return;
    }

    AsyncConnection::on_handle_read(ec, bytes_transferred);
  }

  void AsyncThriftClient::on_handle_write(
    const boost::system::error_code& ec, size_t bytes_transferred)
  {
    if (!pending_async_op_
      || async_op_list_.empty()
      || pending_async_op_.get() != async_op_list_.front().get())
      //operation has been already canceled
      return;

    if (ec)
    {
      on_close(&ec);
      return;
    }

    if (pending_async_op_->is_oneway)
    {
      async_op_list_.pop_front();
      //invoke callback successfully
      boost::shared_ptr<AsyncOp> op;
      op.swap(pending_async_op_);
      op->callback(ec);
    }
    else
    {
      start_recv(true);
    }
  }

  void AsyncThriftClient::on_handle_frame()
  {
    assert(pending_async_op_ && !async_op_list_.empty()
      && pending_async_op_.get() == async_op_list_.front().get());

    async_op_list_.pop_front();

    boost::system::error_code ec;
    try
    {
      fill_result(*pending_async_op_);//may throw
      ec.assign(boost::system::posix_error::success, boost::system::get_posix_category());
    }
    catch (std::exception& e)
    {
      GlobalOutput.printf("caught an exception in AsyncThriftClient::fill_result: %s", e.what());
      ec.assign(boost::system::posix_error::bad_message, boost::system::get_posix_category());
    }

    boost::shared_ptr<AsyncOp> op;
    op.swap(pending_async_op_);
    op->callback(ec);
  }

} } } // namespace
