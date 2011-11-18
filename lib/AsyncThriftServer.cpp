/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <assert.h>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <Thrift.h>
#include <transport/TBufferTransports.h>
#include <protocol/TBinaryProtocol.h>
#include "AsyncThriftServer.h"

namespace apache { namespace thrift { namespace async {

  using ::apache::thrift::GlobalOutput;

  /************************************************************************/
  class AsyncThriftServer::Connection :
    public boost::enable_shared_from_this<AsyncThriftServer::Connection>
  {
  public:
    Connection(boost::asio::io_service& io_service,
      AsyncThriftServer * server)
      :parent(server)
    {
      assert(server);

      socket.reset(new boost::asio::ip::tcp::socket(io_service));

      buffer.resize(kBufferSize);
      bytes_recv = 0;
      frame_size = 0;
      state = kReadFrameSize;

      input_buffer.reset(new ::apache::thrift::transport::TMemoryBuffer);
      output_buffer.reset(new ::apache::thrift::transport::TMemoryBuffer);
      input_framed.reset(new ::apache::thrift::transport::TFramedTransport(input_buffer));
      output_framed.reset(new ::apache::thrift::transport::TFramedTransport(output_buffer));
      input_proto.reset(new ::apache::thrift::protocol::TBinaryProtocol(input_framed));
      output_proto.reset(new ::apache::thrift::protocol::TBinaryProtocol(output_framed));
    }

    ~Connection()
    {
      close();
    }

  private:
    static const size_t kBufferSize = 512;
    static const uint32_t kMaxFrameSize = 1024 * 1024;

    enum kState
    {
      kReadFrameSize,
      kReadFrame,
    };

    boost::shared_ptr<boost::asio::ip::tcp::socket> socket;
    std::vector<uint8_t> buffer;
    uint32_t bytes_recv;
    uint32_t frame_size;
    kState state;
    AsyncThriftServer * parent;

    boost::shared_ptr< ::apache::thrift::transport::TMemoryBuffer> input_buffer;
    boost::shared_ptr< ::apache::thrift::transport::TMemoryBuffer> output_buffer;
    boost::shared_ptr< ::apache::thrift::transport::TFramedTransport> input_framed;
    boost::shared_ptr< ::apache::thrift::transport::TFramedTransport> output_framed;
    boost::shared_ptr< ::apache::thrift::protocol::TProtocol> input_proto;
    boost::shared_ptr< ::apache::thrift::protocol::TProtocol> output_proto;

  public:
    void start(bool restart)
    {
      if (restart)
      {
        //here is the change to shrink the buffer
        std::vector<uint8_t>(kBufferSize).swap(buffer);
        bytes_recv = 0;
        frame_size = 0;
        state = kReadFrameSize;

        socket->async_read_some(
          boost::asio::buffer(buffer),
          boost::bind(&AsyncThriftServer::Connection::handle_read,
          shared_from_this(), _1, _2));
      }
      else
      {
        size_t buffer_size = buffer.size();
        //extend buffer if needed
        if (bytes_recv == buffer_size)
          buffer.resize(buffer_size << 1);

        socket->async_read_some(
          boost::asio::buffer(&buffer[bytes_recv], buffer_size - bytes_recv),
          boost::bind(&AsyncThriftServer::Connection::handle_read,
          shared_from_this(), _1, _2));
      }
    }

    boost::asio::ip::tcp::socket& get_socket()
    {
      return *socket;
    }

  private:
    void close()
    {
      if (socket)
      {
        parent->remove_client(shared_from_this());
        boost::system::error_code ec;
        socket->close(ec);
        socket.reset();
      }
    }

    void get_frame_size()
    {
      assert(buffer.size() >= sizeof(uint32_t));
      frame_size = *(reinterpret_cast<const uint32_t*>(&buffer[0]));
      frame_size = ntohl(frame_size);
    }

    void handle_read(const boost::system::error_code& ec, size_t bytes_transferred)
    {
      if (ec)
      {
        close();
        return;
      }

      bytes_recv += bytes_transferred;
      assert(bytes_recv <= buffer.size());

      handle_buffer();
    }

    void handle_buffer()
    {
      switch (state)
      {
      case kReadFrameSize:
        if (bytes_recv >= sizeof(uint32_t))
        {
          //got the frame length
          get_frame_size();

          if (frame_size >= kMaxFrameSize || frame_size == 0)
          {
            GlobalOutput.printf("illegal frame size: %u", frame_size);
            close();
            return;
          }

          //fall through to "case kReadFrame"
          //the buffer may contains a complete frame, or a partial frame
          state = kReadFrame;
        }
        else
        {
          //continue to read
          start(false);
          break;
        }

      case kReadFrame:
        if (bytes_recv >= frame_size+sizeof(uint32_t))
        {
          //got a complete frame, handle it and reply
          input_buffer->resetBuffer(&buffer[0], frame_size+sizeof(uint32_t));
          output_buffer->resetBuffer();

          try
          {
            parent->getProcessor()->process(input_proto, output_proto);
          }
          catch (...)
          {
            GlobalOutput.printf("caught an exception in Processor::process");
            close();
            return;
          }

          uint32_t out_frame_size;
          uint8_t * out_frame;
          output_buffer->getBuffer(&out_frame, &out_frame_size);

          boost::asio::async_write(*socket,
            boost::asio::buffer(out_frame, out_frame_size),
            boost::asio::transfer_all(),//transfer_all
            boost::bind(&AsyncThriftServer::Connection::handle_write, shared_from_this(), _1, _2));
        }
        else
        {
          //continue to read
          start(false);
        }
        break;
      }
    }

    void handle_write(const boost::system::error_code& ec, size_t /*bytes_transferred*/)
    {
      if (ec)
      {
        close();
        return;
      }

      assert(bytes_recv >= frame_size+sizeof(uint32_t));

      if (bytes_recv == frame_size+sizeof(uint32_t))
      {
        //buffer is empty, restart
        start(true);
      }
      else if (bytes_recv > frame_size+sizeof(uint32_t))
      {
        //consume the previous frame buffer, and handle the buffer remained
        memcpy(&buffer[0], &buffer[frame_size+sizeof(uint32_t)],
          bytes_recv-frame_size-sizeof(uint32_t));
        bytes_recv -= (frame_size+sizeof(uint32_t));
        frame_size = 0;
        state = kReadFrameSize;
        handle_buffer();
      }
    }
  };

  /************************************************************************/
  AsyncThriftServer::AsyncThriftServer(
    boost::shared_ptr< ::apache::thrift::TProcessor> processor,
    const boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor,
    size_t thread_pool_size,
    size_t max_client)
    :TServer(processor), io_service_(acceptor->get_io_service()),
    thread_pool_size_(thread_pool_size?thread_pool_size:1),
    max_client_(max_client)
  {
    assert(acceptor);
    acceptor_ = acceptor;
  }

  AsyncThriftServer::~AsyncThriftServer()
  {
    stop();//NOTICE: no polymorphisms here

    //close server socket
    boost::system::error_code ec;
    acceptor_->close(ec);

    client_.clear();
  }

  void AsyncThriftServer::serve()
  {
    io_service_.reset();

    async_accept();

    boost::thread_group tg;
    for (size_t i=0; i<thread_pool_size_; i++)
      tg.create_thread(boost::bind(&boost::asio::io_service::run, &io_service_));

    tg.join_all();
  }

  void AsyncThriftServer::stop()
  {
    io_service_.stop();
  }

  void AsyncThriftServer::async_accept()
  {
    ConnectionSP conn(new Connection(io_service_, this));

    //pass "conn" to a bound handler that could hold a copy of "conn",
    //which makes it still alive
    acceptor_->async_accept(conn->get_socket(),
      boost::bind(&AsyncThriftServer::handle_accept, this, conn, _1));
  }

  void AsyncThriftServer::handle_accept(ConnectionSP conn,
    const boost::system::error_code& ec)
  {
    if (!ec)
    {
      client_.insert(conn);
      conn->start(true);

      if (max_client_ == 0)
        async_accept();
      else
      {
        boost::mutex::scoped_lock guard(client_mutex_);
        if (client_.size() < max_client_)
          async_accept();
      }
    }
  }

  void AsyncThriftServer::remove_client(const ConnectionSP& conn)
  {
    boost::mutex::scoped_lock guard(client_mutex_);
    client_.erase(conn);

    if (max_client_ && client_.size() < max_client_)
      async_accept();
  }

} } } // namespace
