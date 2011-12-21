/** @file
* @brief basic asynchronous connection
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <AsyncConnection.h>

namespace apache { namespace thrift { namespace async {

  AsyncConnection::AsyncConnection()
    :io_service_(0)
  {
    common_init();
  }

  AsyncConnection::AsyncConnection(boost::asio::io_service& io_service)
    :io_service_(&io_service)
  {
    socket_.reset(new boost::asio::ip::tcp::socket(io_service));
    common_init();
  }

  AsyncConnection::AsyncConnection(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket)
    :io_service_(&socket->get_io_service()), socket_(socket)
  {
    common_init();
  }

  AsyncConnection::~AsyncConnection()
  {
  }

  bool AsyncConnection::is_open()const
  {
    return socket_ && socket_->is_open();
  }

  void AsyncConnection::close()
  {
    if (socket_)
    {
      io_service_ = 0;
      boost::system::error_code ec;
      socket_->close(ec);//close it manually
      socket_.reset();
      strand_.reset();
    }
  }

  void AsyncConnection::cancel()
  {
    if (socket_)
    {
      boost::system::error_code ec;
      socket_->cancel(ec);
    }
  }

  void AsyncConnection::attach(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket)
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
      std::vector<uint8_t>(kBufferSize).swap(recv_buffer_);
      bytes_recv_ = 0;
      frame_size_ = 0;
      state_ = kReadFrameSize;

      if (strand_)
      {
        socket_->async_read_some(
          boost::asio::buffer(recv_buffer_),
          strand_->wrap(boost::bind(&AsyncConnection::handle_read,
          shared_from_this(), _1, _2)));
      }
      else
      {
        socket_->async_read_some(
          boost::asio::buffer(recv_buffer_),
          boost::bind(&AsyncConnection::handle_read,
          shared_from_this(), _1, _2));
      }
    }
    else
    {
      size_t buffer_size = recv_buffer_.size();
      //extend buffer if needed
      if (bytes_recv_ == buffer_size)
        recv_buffer_.resize(buffer_size << 1);

      if (strand_)
      {
        socket_->async_read_some(
          boost::asio::buffer(&recv_buffer_[bytes_recv_], buffer_size - bytes_recv_),
          strand_->wrap(boost::bind(&AsyncConnection::handle_read,
          shared_from_this(), _1, _2)));
      }
      else
      {
        socket_->async_read_some(
          boost::asio::buffer(&recv_buffer_[bytes_recv_], buffer_size - bytes_recv_),
          boost::bind(&AsyncConnection::handle_read,
          shared_from_this(), _1, _2));
      }
    }
  }

  void AsyncConnection::common_init()
  {
    recv_buffer_.resize(kBufferSize);
    bytes_recv_ = 0;
    frame_size_ = 0;
    state_ = kReadFrameSize;

    input_buffer_.reset(new TMemoryBuffer);
    output_buffer_.reset(new TMemoryBuffer);
    input_framed_.reset(new TFramedTransport(input_buffer_));
    output_framed_.reset(new TFramedTransport(output_buffer_));
    input_proto_.reset(new TBinaryProtocol(input_framed_));
    output_proto_.reset(new TBinaryProtocol(output_framed_));
  }

  void AsyncConnection::get_frame_size()
  {
    assert(recv_buffer_.size() >= sizeof(uint32_t));
    frame_size_ = *(reinterpret_cast<const uint32_t*>(&recv_buffer_[0]));
    frame_size_ = ntohl(frame_size_);
  }

  void AsyncConnection::start_write_output_buffer()
  {
    uint32_t out_frame_size;
    uint8_t * out_frame;
    output_buffer_->getBuffer(&out_frame, &out_frame_size);//not throw

    if (strand_)
    {
      boost::asio::async_write(*socket_,
        boost::asio::buffer(out_frame, out_frame_size),
        boost::asio::transfer_all(),//transfer_all
        strand_->wrap(boost::bind(&AsyncConnection::handle_write, shared_from_this(), _1, _2)));
    }
    else
    {
      boost::asio::async_write(*socket_,
        boost::asio::buffer(out_frame, out_frame_size),
        boost::asio::transfer_all(),//transfer_all
        boost::bind(&AsyncConnection::handle_write, shared_from_this(), _1, _2));
    }
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
        input_buffer_->resetBuffer(&recv_buffer_[0], frame_size_+sizeof(uint32_t));
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
  }

  void AsyncConnection::on_attach(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket)
  {
    assert(socket);

    close();

    io_service_ = &socket->get_io_service();
    socket_ = socket;
    strand_.reset();
  }

  void AsyncConnection::on_detach()
  {
    if (!socket_)
      return;

    io_service_ = 0;
    socket_.reset();
    strand_.reset();
  }

  void AsyncConnection::on_handle_read(
    const boost::system::error_code& ec, size_t bytes_transferred)
  {
    if (ec)
      return;

    bytes_recv_ += bytes_transferred;
    assert(bytes_recv_ <= recv_buffer_.size());
    handle_buffer();
  }

  void AsyncConnection::on_handle_write(
    const boost::system::error_code& ec, size_t bytes_transferred)
  {
    if (ec)
      return;

    assert(bytes_recv_ >= frame_size_+sizeof(uint32_t));
    if (bytes_recv_ == frame_size_+sizeof(uint32_t))
    {
      //buffer is empty, restart
      start_recv(true);
    }
    else
    {
      //consume the previous frame buffer, and handle the buffer remained
      memcpy(&recv_buffer_[0], &recv_buffer_[frame_size_+sizeof(uint32_t)],
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

  void AsyncConnection::async_process(const boost::system::error_code& ec, bool is_oneway)
  {
    on_async_process(ec, is_oneway);
  }

  void AsyncConnection::on_async_process(const boost::system::error_code& ec, bool is_oneway)
  {
  }

} } } // namespace
