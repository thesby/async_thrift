/** @file
 * @brief base class for asynchronous thrift client
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include "async_client.h"

//lint -esym(578,socket) symbol hides symbol

namespace apache { namespace thrift { namespace async {

  void AsyncThriftClient::complete_pending_op(const boost::system::error_code& ec)
  {
    assert(pending_async_op_);

    boost::shared_ptr<AsyncOp> op;
    op.swap(pending_async_op_);
    op->callback(ec);
  }

  AsyncThriftClient::AsyncThriftClient()
    : BaseType()
  {
  }

  AsyncThriftClient::AsyncThriftClient(
      const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket)
    : BaseType(socket)
  {
  }

  AsyncThriftClient::~AsyncThriftClient()
  {
    assert(!pending_async_op_);
  }

  void AsyncThriftClient::attach(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket)
  {
    if (pending_async_op_)
      throw make_error_code(kThriftHasPendingOp);

    assert(socket);

    close();

    io_service_ = &socket->get_io_service();
    socket_ = socket;
    strand_.reset();
  }

  void AsyncThriftClient::detach()
  {
    if (pending_async_op_)
      throw make_error_code(kThriftHasPendingOp);

    if (!socket_)
      return;

    io_service_ = 0;
    socket_.reset();
    strand_.reset();
  }

  void AsyncThriftClient::on_close(const boost::system::error_code& ec)
  {
    BaseType::on_close(ec);

    if (ec)
    {
      complete_pending_op(ec);
    }
    else
    {
      assert(0);
    }
  }

  void AsyncThriftClient::on_handle_read(
      const boost::system::error_code& ec, size_t bytes_transferred)
  {
    assert(pending_async_op_);

    if (ec)
    {
      on_close(ec);
      return;
    }

    BaseType::on_handle_read(ec, bytes_transferred);
  }

  void AsyncThriftClient::on_handle_write(
      const boost::system::error_code& ec, size_t bytes_transferred)
  {
    assert(pending_async_op_);
    (void)bytes_transferred;

    if (ec)
    {
      on_close(ec);
      return;
    }

    if (pending_async_op_->is_oneway)
    {
      // invoke callback successfully
      complete_pending_op(ec);
    }
    else
    {
      start_recv(true);
    }
  }

  void AsyncThriftClient::on_handle_frame()
  {
    boost::system::error_code ec;
    try
    {
      fill_result(*pending_async_op_);// may throw
      ec.assign(boost::system::posix_error::success, boost::system::get_posix_category());
    }
    catch (TApplicationException& e)
    {
      GlobalOutput.printf("on_handle_frame: %s", e.what());
      ec = make_error_code(e);
    }
    catch (TProtocolException& e)
    {
      GlobalOutput.printf("on_handle_frame: %s", e.what());
      ec = make_error_code(e);
    }
    catch (TTransportException& e)
    {
      GlobalOutput.printf("on_handle_frame: %s", e.what());
      ec = make_error_code(e);
    }
    catch (TException& e)
    {
      GlobalOutput.printf("on_handle_frame: %s", e.what());
      ec = make_error_code(e);
    }
    catch (std::exception& e)
    {
      GlobalOutput.printf("on_handle_frame: %s", e.what());
      ec.assign(boost::system::posix_error::bad_message, boost::system::get_posix_category());
    }
    catch (...)
    {
      GlobalOutput.printf("on_handle_frame: error");
      ec.assign(boost::system::posix_error::bad_message, boost::system::get_posix_category());
    }

    complete_pending_op(ec);
  }

} } }
