/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <assert.h>
#include <boost/bind.hpp>
#include <Thrift.h>
#include <transport/TTransportException.h>
#include <protocol/TBinaryProtocol.h>
#include <AsyncThriftClient.h>

namespace apache { namespace thrift { namespace async {

  using ::apache::thrift::GlobalOutput;

  /************************************************************************/
  AsyncThriftClient::AsyncThriftClient()
  {
  }

  AsyncThriftClient::AsyncThriftClient(const boost::shared_ptr<boost::asio::ip::tcp::socket> socket)
    :AsyncConnection(socket), timeout_(0)
  {
    timer_.reset(new boost::asio::deadline_timer(get_io_service()));
  }

  AsyncThriftClient::~AsyncThriftClient()
  {
    AsyncThriftClient::on_close(0);
  }

  void AsyncThriftClient::set_rpc_timer()
  {
    if (timeout_)
    {
      boost::posix_time::milliseconds timeout(timeout_);
      timer_->expires_from_now(timeout);
      timer_->async_wait(strand_->wrap(boost::bind(&AsyncThriftClient::handle_timeout, this, _1)));
    }
  }

  void AsyncThriftClient::cancel_rpc_timer()
  {
    boost::system::error_code ec;
    timer_->cancel(ec);
  }

  void AsyncThriftClient::handle_timeout(const boost::system::error_code& ec)
  {
    if (!ec && timer_->expires_at() <= boost::asio::deadline_timer::traits_type::now())
    {
      //The deadline has passed
      boost::system::error_code timeout(
        boost::system::posix_error::timed_out, boost::system::get_posix_category());
      on_close(&timeout);
    }
  }

  void AsyncThriftClient::on_close(const boost::system::error_code * ec)
  {
    if (!socket_)
      return;

    io_service_ = 0;
    strand_.reset();
    cancel_rpc_timer();
    timer_.reset();
    {
      boost::system::error_code ec;
      socket_->close(ec);
    }
    socket_.reset();

    boost::system::error_code real_ec;
    if (ec)
      real_ec = *ec;
    else
      real_ec.assign(
      boost::system::posix_error::operation_canceled, boost::system::get_posix_category());

    //execute the pending and queued callback if they exist
    pending_async_op_.reset();
    boost::shared_ptr<AsyncOp> op;
    while (!async_op_list_.empty())
    {
      op = async_op_list_.front();
      async_op_list_.pop_front();
      assert(op);
      op->callback(real_ec);
    }
  }

  void AsyncThriftClient::on_attach(const boost::shared_ptr<boost::asio::ip::tcp::socket> socket)
  {
    close();
    assert(socket);

    io_service_ = &socket->get_io_service();
    socket_ = socket;
    strand_.reset(new boost::asio::io_service::strand(get_io_service()));
    timer_.reset(new boost::asio::deadline_timer(get_io_service()));
  }

  void AsyncThriftClient::on_detach()
  {
    if (!socket_)
      return;

    if (async_op_list_.empty())
    {
      io_service_ = 0;
      strand_.reset();
      timer_.reset();
      socket_.reset();
    }
    else
    {
      throw ::apache::thrift::transport::TTransportException(
        "can not detach a socket that has pending asynchronous operations");
    }
  }

  void AsyncThriftClient::on_handle_read(const boost::system::error_code& ec, size_t bytes_transferred)
  {
    if (!pending_async_op_
      || async_op_list_.empty()
      || pending_async_op_.get() != async_op_list_.front().get())
      //operation has been canceled and async_op_list_ has been cleared because of timeout or network errors
      return;

    if (ec)
    {
      async_op_list_.pop_front();

      boost::shared_ptr<AsyncOp> op;
      op.swap(pending_async_op_);
      op->callback(ec);
      op.reset();

      on_close(&ec);
      return;
    }

    AsyncConnection::on_handle_read(ec, bytes_transferred);
  }

  void AsyncThriftClient::on_handle_write(const boost::system::error_code& ec, size_t bytes_transferred)
  {
    if (!pending_async_op_
      || async_op_list_.empty()
      || pending_async_op_.get() != async_op_list_.front().get())
      return;

    if (ec)
    {
      async_op_list_.pop_front();

      boost::shared_ptr<AsyncOp> op;
      op.swap(pending_async_op_);
      op->callback(ec);
      op.reset();

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
      op.reset();
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
    cancel_rpc_timer();
    fill_result(*pending_async_op_);
    //invoke callback successfully
    boost::system::error_code ec(
      boost::system::posix_error::success, boost::system::get_posix_category());

    boost::shared_ptr<AsyncOp> op;
    op.swap(pending_async_op_);
    op->callback(ec);
    op.reset();
  }

} } } // namespace
