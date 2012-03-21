/** @file
* @brief basic asynchronous connection
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_CONNECTION_H
#define ASYNC_CONNECTION_H

#include <AsyncCommon.h>

namespace apache { namespace thrift { namespace async {

  std::string dump_address(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket);

  /*
  * AsyncConnection(using TFramedTransport, TBinaryProtocol)
  *
  * socket layer asynchronous
  * both server and client re-use this code as base class
  */
  class AsyncConnection : private boost::noncopyable,
    public boost::enable_shared_from_this<AsyncConnection>
  {
  protected:
    static const size_t kBufferSize = 4 * 1024;
    static const size_t kFrameSize = sizeof(uint32_t);
    //NOTICE: RPC packet size must not be greater than kMaxFrameSize.
    //100MB is a large limitation, modify it manually if you may break it.
    static const uint32_t kMaxFrameSize = 100 * 1024 * 1024;

    enum kState
    {
      kReadFrameSize,
      kReadFrame,
    };

    //about asio
    boost::asio::io_service * io_service_;
    boost::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    boost::shared_ptr<boost::asio::io_service::strand> strand_;

    //about buffer
    std::vector<uint8_t> recv_buffer_;
    uint32_t bytes_recv_;
    uint32_t frame_size_;
    kState state_;

    //about thrift
    boost::shared_ptr<TMemoryBuffer> input_buffer_;
    boost::shared_ptr<TMemoryBuffer> output_buffer_;
    boost::shared_ptr<TFramedTransport> input_framed_;
    boost::shared_ptr<TFramedTransport> output_framed_;
    boost::shared_ptr<TProtocol> input_proto_;
    boost::shared_ptr<TProtocol> output_proto_;

  public:
    AsyncConnection();
    explicit AsyncConnection(boost::asio::io_service& io_service);
    explicit AsyncConnection(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket);
    virtual ~AsyncConnection();

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

    //NOTICE:
    //If needed, 'set_strand' must be invoked before any operations.
    //Usually, we use one 'strand' object during one conceptual 'session'
    //to synchronize handler's invocation.
    boost::shared_ptr<boost::asio::io_service::strand>& get_strand()
    {
      return strand_;
    }

    void set_strand(const boost::shared_ptr<boost::asio::io_service::strand>& strand)
    {
      strand_ = strand;
    }

    bool is_open()const;
    void close();
    //Cancel asynchronous operations
    void cancel();

    //NOTICE:
    //If there are pending asynchronous operations, attach and detach may throw.
    //attach shall close the previous inner 'socket_' and release the previous inner 'strand_'.
    void attach(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket);
    void detach();

    //start asynchronous receiving
    //restart is true, restart a new recveive
    //restart is false, continue to recveive
    void start_recv(bool restart);

  private:
    void common_init();

  protected:
    void get_frame_size();
    void handle_read(const boost::system::error_code& ec, size_t);
    void handle_write(const boost::system::error_code& ec, size_t);
    void handle_buffer();
    void async_process(const boost::system::error_code& ec, bool is_oneway);
    //retrieve all stuffs in 'output_buffer_' and start asynchronous writing
    void start_write_output_buffer();

  protected:
    //virtual functions
    //must not throw
    virtual void on_close(const boost::system::error_code * ec);
    //may throw, if there are pending asynchronous operations
    virtual void on_attach(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket);
    //may throw, if there are pending asynchronous operations
    virtual void on_detach();
    //must not throw
    virtual void on_handle_read(const boost::system::error_code& ec, size_t);
    //must not throw
    virtual void on_handle_write(const boost::system::error_code& ec, size_t);
    //must not throw
    virtual void on_handle_frame();
    //must not throw
    virtual void on_async_process(const boost::system::error_code& ec, bool is_oneway);
  };

} } } // namespace

#endif
