/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_CONNECTION_H
#define ASYNC_CONNECTION_H

#include "AsyncCommon.h"

namespace apache { namespace thrift { namespace async {

  /*
  * AsyncConnection(using TFramedTransport, TBinaryProtocol)
  *
  * socket layer asynchronous
  *
  * Single Thread!!!
  */
  class AsyncConnection : private boost::noncopyable,
    public boost::enable_shared_from_this<AsyncConnection>
  {
  protected:
    static const size_t kBufferSize = 512;
    static const uint32_t kMaxFrameSize = 1024 * 1024;

    enum kState
    {
      kReadFrameSize,
      kReadFrame,
    };

    boost::asio::io_service * io_service_;
    boost::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    boost::shared_ptr<boost::asio::io_service::strand> strand_;
    std::vector<uint8_t> recv_buffer_;
    uint32_t bytes_recv_;
    uint32_t frame_size_;
    kState state_;

    boost::shared_ptr<TMemoryBuffer> input_buffer_;
    boost::shared_ptr<TMemoryBuffer> output_buffer_;
    boost::shared_ptr<TFramedTransport> input_framed_;
    boost::shared_ptr<TFramedTransport> output_framed_;
    boost::shared_ptr<TProtocol> input_proto_;
    boost::shared_ptr<TProtocol> output_proto_;

  public:
    AsyncConnection();
    //server side:
    //used with an tcp acceptor
    explicit AsyncConnection(boost::asio::io_service& io_service);
    //client side:
    //socket must be opened
    //this kind of constructor may help users adapt a connection pool with this class
    explicit AsyncConnection(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket);
    virtual ~AsyncConnection();

    boost::asio::io_service& get_io_service()
    {
      return *io_service_;
    }

    boost::asio::ip::tcp::socket& get_socket()
    {
      return *socket_;
    }

    //NOTICE: if we use timer, create_strand must be invoked before any operations
    void create_strand()
    {
      strand_.reset(new boost::asio::io_service::strand(get_io_service()));
    }

    boost::shared_ptr<boost::asio::io_service::strand> get_strand()
    {
      return strand_;
    }

    void set_strand(boost::shared_ptr<boost::asio::io_service::strand> strand)
    {
      strand_ = strand;
    }

    bool is_open()const;
    //close the inner socket, clear all asynchronous operations,
    //whose callback shall be invoked with the "ec" as "operation canceled"
    void close();
    //NOTICE: attach shall close the inner socket
    void attach(const boost::shared_ptr<boost::asio::ip::tcp::socket> socket);
    //NOTICE: detach shall not close the inner socket
    void detach();

    void start_recv(bool restart);

    void start_write_output_buffer();

  protected:
    void common_init();
    void get_frame_size();
    void handle_read(const boost::system::error_code& ec, size_t bytes_transferred);
    void handle_write(const boost::system::error_code& ec, size_t bytes_transferred);
    void handle_buffer();

    virtual void on_close(const boost::system::error_code * ec);
    virtual void on_attach(const boost::shared_ptr<boost::asio::ip::tcp::socket> socket);
    virtual void on_detach();
    virtual void on_handle_read(const boost::system::error_code& ec, size_t bytes_transferred);
    virtual void on_handle_write(const boost::system::error_code& ec, size_t bytes_transferred);
    virtual void on_handle_frame();
  };

} } } // namespace

#endif
