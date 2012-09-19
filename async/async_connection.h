/** @file
 * @brief basic asynchronous connection
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef ASYNC_CONNECTION_H
#define ASYNC_CONNECTION_H

#include <async_exception.h>
#include <async_util.h>

namespace apache { namespace thrift { namespace async {

  /*
   * AsyncConnection(using TFramedTransport, TBinaryProtocol)
   *
   * socket layer asynchronous
   * both server and client re-use this code as base class
   */
  class AsyncConnection : private boost::noncopyable
  {
    protected:
      static const size_t kBufferSize = 4 * 1024;
      static const size_t kFrameSize = sizeof(uint32_t);
      // NOTICE: RPC packet size must not be greater than kMaxFrameSize.
      // 100MB is a large limitation, modify it manually if you may break it.
      static const uint32_t kMaxFrameSize = 100 * 1024 * 1024;

      enum kState
      {
        kReadFrameSize,
        kReadFrame,
      };

      // about asio
      boost::asio::io_service * io_service_;
      boost::shared_ptr<boost::asio::ip::tcp::socket> socket_;
      boost::shared_ptr<boost::asio::io_service::strand> strand_;

      // about buffer
      std::vector<uint8_t> recv_buffer_;
      uint32_t bytes_recv_;
      uint32_t frame_size_;
      kState state_;

      // about thrift
      boost::shared_ptr<TMemoryBuffer> input_buffer_;
      boost::shared_ptr<TMemoryBuffer> output_buffer_;
      boost::shared_ptr<TFramedTransport> input_framed_;
      boost::shared_ptr<TFramedTransport> output_framed_;
      boost::shared_ptr<TProtocol> input_proto_;
      boost::shared_ptr<TProtocol> output_proto_;

    public:
      AsyncConnection()
        :io_service_(0)
      {
        common_init();
      }

      explicit AsyncConnection(boost::asio::io_service& io_service)
        :io_service_(&io_service)
      {
        socket_.reset(new boost::asio::ip::tcp::socket(io_service));
        common_init();
      }

      explicit AsyncConnection(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket)
        :io_service_(&socket->get_io_service()), socket_(socket)
      {
        common_init();
      }

      virtual ~AsyncConnection()
      {
      }

      boost::asio::io_service& get_io_service()
      {
        assert(io_service_);
        return *io_service_;
      }

      boost::asio::ip::tcp::socket& get_socket()
      {
        assert(socket_);
        return *socket_;
      }

      // NOTICE:
      // If needed, 'set_strand' must be invoked before any operations.
      // Usually, we use one 'strand' object during one conceptual 'session'
      // to synchronize handler's invocation.
      boost::shared_ptr<boost::asio::io_service::strand>& get_strand()
      {
        return strand_;
      }

      void set_strand(const boost::shared_ptr<boost::asio::io_service::strand>& strand)
      {
        strand_ = strand;
      }

      bool is_open()const
      {
        return socket_ && socket_->is_open();
      }

      void close()
      {
        if (socket_ && socket_->is_open())
          socket_->close();
      }

      // start asynchronous receiving
      // restart is true, restart a new receive
      // restart is false, continue to receive
      void start_recv(bool restart)
      {
        if (restart)
        {
          if (recv_buffer_.size() > kMaxFrameSize)
          {
            // This is the only chance to shrink recv_buffer_
            std::vector<uint8_t>(kBufferSize).swap(recv_buffer_);
          }
          bytes_recv_ = 0;
          frame_size_ = 0;
          state_ = kReadFrameSize;

          if (strand_)
          {
            socket_->async_read_some(
                boost::asio::buffer(recv_buffer_),
                strand_->wrap(boost::bind(&AsyncConnection::handle_read,
                    this, _1, _2)));
          }
          else
          {
            socket_->async_read_some(
                boost::asio::buffer(recv_buffer_),
                boost::bind(&AsyncConnection::handle_read,
                  this, _1, _2));
          }
        }
        else
        {
          if (state_ == kReadFrame)
            recv_buffer_.resize(frame_size_ + kFrameSize);

          size_t buffer_size = recv_buffer_.size();
          if (strand_)
          {
            socket_->async_read_some(
                boost::asio::buffer(&recv_buffer_[bytes_recv_], buffer_size - bytes_recv_),
                strand_->wrap(boost::bind(&AsyncConnection::handle_read,
                    this, _1, _2)));
          }
          else
          {
            socket_->async_read_some(
                boost::asio::buffer(&recv_buffer_[bytes_recv_], buffer_size - bytes_recv_),
                boost::bind(&AsyncConnection::handle_read,
                  this, _1, _2));
          }
        }
      }

    private:
      void common_init()
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

    protected:
      void get_frame_size()
      {
        frame_size_ = *(reinterpret_cast<const uint32_t*>(&recv_buffer_[0]));
        frame_size_ = ntohl(frame_size_);
      }

      void handle_read(const boost::system::error_code& ec, size_t bytes_transferred)
      {
        on_handle_read(ec, bytes_transferred);
      }

      void handle_write(const boost::system::error_code& ec, size_t bytes_transferred)
      {
        on_handle_write(ec, bytes_transferred);
      }

      void handle_buffer()
      {
        switch (state_)
        {
          case kReadFrameSize:
            if (bytes_recv_ >= kFrameSize)
            {
              // got the frame length
              get_frame_size();

              if (frame_size_ >= kMaxFrameSize || frame_size_ == 0)
              {
                GlobalOutput.printf("%s illegal frame size: %u",
                    socket_address_to_string(socket_).c_str(), frame_size_);

                boost::system::error_code ec;
                if (frame_size_ >= kMaxFrameSize)
                  ec = make_error_code(kProtoSizeLimit);
                else
                  ec = make_error_code(kProtoNegativeSize);
                on_close(ec);// current connection diminishes here
                return;
              }

              // fall through to "case kReadFrame"
              // the buffer may contains a complete frame, or a partial frame
              state_ = kReadFrame;
            }
            else
            {
              // continue to read
              start_recv(false);
              break;
            }

          case kReadFrame:
            if (bytes_recv_ >= frame_size_+kFrameSize)
            {
              // got a complete frame, handle it
              input_buffer_->resetBuffer(&recv_buffer_[0], frame_size_+kFrameSize);
              output_buffer_->resetBuffer();
              on_handle_frame();
            }
            else
            {
              // continue to read
              start_recv(false);
            }
            break;
        }
      }

      // retrieve all stuffs in 'output_buffer_' and start asynchronous writing
      void start_write_output_buffer()
      {
        uint32_t out_frame_size;
        uint8_t * out_frame;
        output_buffer_->getBuffer(&out_frame, &out_frame_size);// not throw

        if (strand_)
        {
          boost::asio::async_write(*socket_,
              boost::asio::buffer(out_frame, out_frame_size),
              boost::asio::transfer_all(),// transfer_all
              strand_->wrap(boost::bind(&AsyncConnection::handle_write, this, _1, _2)));
        }
        else
        {
          boost::asio::async_write(*socket_,
              boost::asio::buffer(out_frame, out_frame_size),
              boost::asio::transfer_all(),// transfer_all
              boost::bind(&AsyncConnection::handle_write, this, _1, _2));
        }
      }

    protected:
      // virtual functions
      // must not throw
      virtual void on_close(const boost::system::error_code& ec)
      {
        if (socket_)
        {
          io_service_ = 0;
          if (socket_->is_open())
          {
            boost::system::error_code _ec;
            socket_->close(_ec);
          }
          socket_.reset();
          strand_.reset();
        }
      }

      // must not throw
      virtual void on_handle_read(const boost::system::error_code& ec, size_t bytes_transferred)
      {
        if (ec)
        {
          on_close(ec);// current connection diminishes here
          return;
        }

        bytes_recv_ += bytes_transferred;
        assert(bytes_recv_ <= recv_buffer_.size());
        handle_buffer();
      }

      // must not throw
      virtual void on_handle_write(const boost::system::error_code& ec, size_t bytes_transferred)
      {
        if (ec)
        {
          on_close(ec);// current connection diminishes here
          return;
        }

        assert(bytes_recv_ >= frame_size_+kFrameSize);
        if (bytes_recv_ == frame_size_+kFrameSize)
        {
          // buffer is empty, restart
          start_recv(true);
        }
        else
        {
          // consume the previous frame buffer, and handle the buffer remained
          memcpy(&recv_buffer_[0], &recv_buffer_[frame_size_+kFrameSize],
              bytes_recv_-frame_size_-kFrameSize);
          bytes_recv_ -= (frame_size_+kFrameSize);
          frame_size_ = 0;
          state_ = kReadFrameSize;
          handle_buffer();
        }
      }

      // must not throw
      virtual void on_handle_frame()
      {
      }
  };

} } } // namespace

#endif
