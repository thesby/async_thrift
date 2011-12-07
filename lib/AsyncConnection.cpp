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
#include <AsyncConnection.h>

namespace apache { namespace thrift { namespace async {

  using ::apache::thrift::GlobalOutput;

  AsyncConnection::AsyncConnection()
    :io_service_(0)
  {
    common_init();
  }

  AsyncConnection::AsyncConnection(boost::asio::io_service& io_service)
    :io_service_(&io_service)
  {
    socket_.reset(new boost::asio::ip::tcp::socket(io_service));
    //strand_.reset(new boost::asio::io_service::strand(get_io_service()));
    common_init();
  }

  AsyncConnection::AsyncConnection(const boost::shared_ptr<boost::asio::ip::tcp::socket> socket)
    :io_service_(&socket->get_io_service()), socket_(socket)
  {
    //strand_.reset(new boost::asio::io_service::strand(get_io_service()));
    common_init();
  }

  void AsyncConnection::common_init()
  {
    buffer_.resize(kBufferSize);
    bytes_recv_ = 0;
    frame_size_ = 0;
    state_ = kReadFrameSize;

    input_buffer_.reset(new ::apache::thrift::transport::TMemoryBuffer);
    output_buffer_.reset(new ::apache::thrift::transport::TMemoryBuffer);
    input_framed_.reset(new ::apache::thrift::transport::TFramedTransport(input_buffer_));
    output_framed_.reset(new ::apache::thrift::transport::TFramedTransport(output_buffer_));
    input_proto_.reset(new ::apache::thrift::protocol::TBinaryProtocol(input_framed_));
    output_proto_.reset(new ::apache::thrift::protocol::TBinaryProtocol(output_framed_));
  }

  AsyncConnection::~AsyncConnection()
  {
    if (socket_)
    {
      boost::system::error_code ec;
      socket_->close(ec);
      socket_.reset();
    }
  }

  bool AsyncConnection::is_open()const
  {
    return socket_ && socket_->is_open();
  }

  void AsyncConnection::close()
  {
    on_close(0);
  }

  void AsyncConnection::attach(const boost::shared_ptr<boost::asio::ip::tcp::socket> socket)
  {
    on_attach(socket);
  }

  void AsyncConnection::detach()
  {
    on_detach();
  }

  void AsyncConnection::start_recv(bool restart)
  {
    if (restart)
    {
      //here is the change to shrink the buffer
      std::vector<uint8_t>(kBufferSize).swap(buffer_);
      bytes_recv_ = 0;
      frame_size_ = 0;
      state_ = kReadFrameSize;

      if (strand_)
      {
        socket_->async_read_some(
          boost::asio::buffer(buffer_),
          strand_->wrap(boost::bind(&AsyncConnection::handle_read,
          shared_from_this(), _1, _2)));
      }
      else
      {
        socket_->async_read_some(
          boost::asio::buffer(buffer_),
          boost::bind(&AsyncConnection::handle_read,
          shared_from_this(), _1, _2));
      }
    }
    else
    {
      size_t buffer_size = buffer_.size();
      //extend buffer if needed
      if (bytes_recv_ == buffer_size)
        buffer_.resize(buffer_size << 1);

      if (strand_)
      {
        socket_->async_read_some(
          boost::asio::buffer(&buffer_[bytes_recv_], buffer_size - bytes_recv_),
          strand_->wrap(boost::bind(&AsyncConnection::handle_read,
          shared_from_this(), _1, _2)));
      }
      else
      {
        socket_->async_read_some(
          boost::asio::buffer(&buffer_[bytes_recv_], buffer_size - bytes_recv_),
          boost::bind(&AsyncConnection::handle_read,
          shared_from_this(), _1, _2));
      }
    }
  }

  void AsyncConnection::get_frame_size()
  {
    assert(buffer_.size() >= sizeof(uint32_t));
    frame_size_ = *(reinterpret_cast<const uint32_t*>(&buffer_[0]));
    frame_size_ = ntohl(frame_size_);
  }

  void AsyncConnection::handle_read(const boost::system::error_code& ec, size_t bytes_transferred)
  {
    on_handle_read(ec, bytes_transferred);
  }

  void AsyncConnection::handle_write(const boost::system::error_code& ec, size_t bytes_transferred)
  {
    on_handle_write(ec, bytes_transferred);
  }

  void AsyncConnection::handle_buffer()
  {
    switch (state_)
    {
    case kReadFrameSize:
      if (bytes_recv_ >= sizeof(uint32_t))
      {
        //got the frame length
        get_frame_size();

        if (frame_size_ >= kMaxFrameSize || frame_size_ == 0)
        {
          GlobalOutput.printf("illegal frame size: %u", frame_size_);
          close();
          return;
        }

        //fall through to "case kReadFrame"
        //the buffer may contains a complete frame, or a partial frame
        state_ = kReadFrame;
      }
      else
      {
        //continue to read
        start_recv(false);
        break;
      }

    case kReadFrame:
      if (bytes_recv_ >= frame_size_+sizeof(uint32_t))
      {
        //got a complete frame, handle it
        input_buffer_->resetBuffer(&buffer_[0], frame_size_+sizeof(uint32_t));
        output_buffer_->resetBuffer();
        on_handle_frame();
      }
      else
      {
        //continue to read
        start_recv(false);
      }
      break;
    }
  }

  void AsyncConnection::on_close(const boost::system::error_code * ec)
  {
    if (socket_)
    {
      boost::system::error_code ec;
      socket_->close(ec);
      socket_.reset();

      strand_.reset();
    }
  }

  void AsyncConnection::on_attach(const boost::shared_ptr<boost::asio::ip::tcp::socket> socket)
  {
    close();
    assert(socket);

    io_service_ = &socket->get_io_service();
    socket_ = socket;
    //strand_.reset(new boost::asio::io_service::strand(get_io_service()));
  }

  void AsyncConnection::on_detach()
  {
    if (!socket_)
      return;

    io_service_ = NULL;
    //strand_.reset();
    socket_.reset();
  }

  void AsyncConnection::on_handle_read(const boost::system::error_code& ec, size_t bytes_transferred)
  {
    if (ec)
    {
      on_close(&ec);
      return;
    }

    bytes_recv_ += bytes_transferred;
    assert(bytes_recv_ <= buffer_.size());

    handle_buffer();
  }

  void AsyncConnection::on_handle_write(const boost::system::error_code& ec, size_t bytes_transferred)
  {
    if (ec)
    {
      on_close(&ec);
      return;
    }

    assert(bytes_recv_ >= frame_size_+sizeof(uint32_t));

    if (bytes_recv_ == frame_size_+sizeof(uint32_t))
    {
      //buffer is empty, restart
      start_recv(true);
    }
    else//if (bytes_recv_ > frame_size_+sizeof(uint32_t))
    {
      //consume the previous frame buffer, and handle the buffer remained
      memcpy(&buffer_[0], &buffer_[frame_size_+sizeof(uint32_t)],
        bytes_recv_-frame_size_-sizeof(uint32_t));
      bytes_recv_ -= (frame_size_+sizeof(uint32_t));
      frame_size_ = 0;
      state_ = kReadFrameSize;
      handle_buffer();
    }
  }

  void AsyncConnection::on_handle_frame()
  {
  }

} } } // namespace
