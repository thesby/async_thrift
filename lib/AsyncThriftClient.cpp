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
#include <protocol/TBinaryProtocol.h>
#include "AsyncThriftClient.h"

namespace apache { namespace thrift { namespace async {

  using ::apache::thrift::GlobalOutput;

  /************************************************************************/
  AsyncThriftClient::AsyncThriftClient()
    :timeout_(0)
  {
    io_service_ = NULL;
    common_init();
  }

  AsyncThriftClient::AsyncThriftClient(
    const boost::shared_ptr<boost::asio::ip::tcp::socket> socket)
    :timeout_(0)
  {
    attach(socket);
    common_init();
  }

  AsyncThriftClient::~AsyncThriftClient()
  {
    close(NULL);
  }

  bool AsyncThriftClient::is_open()const
  {
    return socket_ && socket_->is_open();
  }

  void AsyncThriftClient::close()
  {
    close(NULL);
  }

  void AsyncThriftClient::attach(
    const boost::shared_ptr<boost::asio::ip::tcp::socket> socket)
  {
    assert(socket);

    close(NULL);

    io_service_ = &socket->get_io_service();
    strand_.reset(new boost::asio::io_service::strand(*io_service_));
    timer_.reset(new boost::asio::deadline_timer(*io_service_));
    socket_ = socket;
  }

  void AsyncThriftClient::detach()
  {
    if (!socket_)
      return;

    if (async_op_list_.empty())
    {
      io_service_ = NULL;
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

  int32_t AsyncThriftClient::get_frame_size(const std::vector<uint8_t>& frame)
  {
    assert(frame.size() >= sizeof(int32_t));
    int32_t size = *(reinterpret_cast<const int32_t*>(&frame[0]));
    size = ntohl(size);
    return size;
  }

  void AsyncThriftClient::close(const boost::system::error_code * ec)
  {
    if (!socket_)
      return;

    io_service_ = NULL;
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

    //execute the pending callback if it exists
    boost::shared_ptr<AsyncOp> op;
    while (!async_op_list_.empty())
    {
      op = async_op_list_.front();
      async_op_list_.pop_front();
      assert(op);
      op->callback(real_ec);
    }
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

  void AsyncThriftClient::common_init()
  {
    input_buffer_.reset(new ::apache::thrift::transport::TMemoryBuffer);
    output_buffer_.reset(new ::apache::thrift::transport::TMemoryBuffer);
    input_framed_.reset(new ::apache::thrift::transport::TFramedTransport(input_buffer_));
    output_framed_.reset(new ::apache::thrift::transport::TFramedTransport(output_buffer_));
    input_proto_.reset(new ::apache::thrift::protocol::TBinaryProtocol(input_framed_));
    output_proto_.reset(new ::apache::thrift::protocol::TBinaryProtocol(output_framed_));
  }

  void AsyncThriftClient::handle_timeout(const boost::system::error_code& ec)
  {
    if (!ec && timer_->expires_at() <= boost::asio::deadline_timer::traits_type::now())
    {
      //The deadline has passed
      boost::system::error_code timeout(
        boost::system::posix_error::timed_out, boost::system::get_posix_category());
      close(&timeout);
    }
  }

  void AsyncThriftClient::handle_read_length(boost::shared_ptr<AsyncOp> op,
    const boost::system::error_code& ec, size_t bytes_transferred)
  {
    assert(op);
    if (async_op_list_.empty() || op.get() != async_op_list_.front().get())
      //operation has been cancelled and async_op_list_ has been cleared because of timeout or disconnection
      return;

    if (ec)
    {
      async_op_list_.pop_front();//pop first
      op->callback(ec);
      close(&ec);
    }
    else
    {
      assert(bytes_transferred == sizeof(int32_t));

      int32_t _return_size = get_frame_size(op->frame);

      if (_return_size <= 0)
      {
        boost::system::error_code bad_message(
          boost::system::posix_error::bad_message, boost::system::get_posix_category());
        async_op_list_.pop_front();
        op->callback(bad_message);
        close(&bad_message);
      }
      else
      {
        //read packet body
        op->frame.resize(sizeof(int32_t) + _return_size);
        boost::asio::async_read(
          *socket_,
          boost::asio::buffer(&op->frame[0] + sizeof(int32_t), _return_size),
          boost::asio::transfer_all(),
          strand_->wrap(boost::bind(&AsyncThriftClient::handle_read_frame, this, op, _1, _2)));
      }
    }
  }

  void AsyncThriftClient::handle_read_frame(boost::shared_ptr<AsyncOp> op,
    const boost::system::error_code& ec, size_t bytes_transferred)
  {
    assert(op);
    if (async_op_list_.empty() || op.get() != async_op_list_.front().get())
      return;

    async_op_list_.pop_front();
    if (ec)
    {
      op->callback(ec);
      close(&ec);
    }
    else
    {
      cancel_rpc_timer();

      //deserilization
      input_buffer_->resetBuffer(&op->frame[0], op->frame.size());
      fill_result(*op);
      //invoke callback successfully
      op->callback(ec);
    }
  }

  void AsyncThriftClient::handle_write(bool is_oneway, boost::shared_ptr<AsyncOp> op,
    const boost::system::error_code& ec, size_t bytes_transferred)
  {
    assert(op);
    if (async_op_list_.empty() || op.get() != async_op_list_.front().get())
      return;

    if (ec)
    {
      async_op_list_.pop_front();
      op->callback(ec);
      close(&ec);
    }
    else
    {
      if (is_oneway)
      {
        async_op_list_.pop_front();
        //invoke callback successfully
        op->callback(ec);
      }
      else
      {
        op->frame.resize(sizeof(int32_t));
        boost::asio::async_read(*socket_,
          boost::asio::buffer(op->frame),
          boost::asio::transfer_all(),
          strand_->wrap(boost::bind(&AsyncThriftClient::handle_read_length, this, op, _1, _2)));
      }
    }
  }

} } } // namespace
