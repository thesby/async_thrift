/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_THRIFT_CLIENT_H
#define ASYNC_THRIFT_CLIENT_H

#include <stdint.h>
#include <vector>
#include <list>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <transport/TBufferTransports.h>
#include <transport/TTransportException.h>
#include <protocol/TProtocol.h>

namespace apache { namespace thrift { namespace async {

  //Single Thread Safe!!!
  class AsyncThriftClient : private boost::noncopyable
  {
  public:
    typedef boost::function<void (const boost::system::error_code& ec)> AsyncRPCCallback;

    AsyncThriftClient();
    //socket must be opened
    //this kind of constructor may help users adapt a connection pool with this class
    explicit AsyncThriftClient(const boost::shared_ptr<boost::asio::ip::tcp::socket> socket);
    virtual ~AsyncThriftClient();

    const boost::asio::ip::tcp::socket& get_socket()const
    {
      return *socket_;
    }

    bool is_open()const;
    //close the inner socket, clear all asynchronous operations,
    //whose callback shall be invoked with the "ec" as "operation canceled"
    void close();
    //NOTICE: attach shall close the inner socket
    void attach(const boost::shared_ptr<boost::asio::ip::tcp::socket> socket);
    //NOTICE: detach shall not close the inner socket
    void detach();

    //0 means no timeout
    //the timeout will take effect after the current operation if existing
    void set_rpc_timeout(size_t milliseconds)
    {
      timeout_ = milliseconds;
    }

    size_t get_rpc_timeout()const
    {
      return timeout_;
    }

    static int32_t get_frame_size(const std::vector<uint8_t>& frame);

  protected:
    struct AsyncOp
    {
      std::vector<uint8_t> frame;
      AsyncRPCCallback callback;
      int rpc_type;
      void * _return;//pointer to various type. if it is NULL, return type is "void"
    };
    std::list<boost::shared_ptr<AsyncOp> > async_op_list_;

  protected:
    void close(const boost::system::error_code * ec);
    void set_rpc_timer();
    void cancel_rpc_timer();
    void common_init();
    void handle_timeout(const boost::system::error_code& ec);
    void handle_read_length(boost::shared_ptr<AsyncOp> op,
      const boost::system::error_code& ec, size_t bytes_transferred);
    void handle_read_frame(boost::shared_ptr<AsyncOp> op,
      const boost::system::error_code& ec, size_t bytes_transferred);
    void handle_write(bool is_oneway, boost::shared_ptr<AsyncOp> op,
      const boost::system::error_code& ec, size_t bytes_transferred);

    virtual void fill_result(AsyncOp& op) = 0;
  protected:
    boost::asio::io_service * io_service_;
    boost::shared_ptr<boost::asio::io_service::strand> strand_;
    boost::shared_ptr<boost::asio::deadline_timer> timer_;
    boost::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    size_t timeout_;

    boost::shared_ptr< ::apache::thrift::transport::TMemoryBuffer> input_buffer_;
    boost::shared_ptr< ::apache::thrift::transport::TMemoryBuffer> output_buffer_;
    boost::shared_ptr< ::apache::thrift::transport::TFramedTransport> input_framed_;
    boost::shared_ptr< ::apache::thrift::transport::TFramedTransport> output_framed_;
    boost::shared_ptr< ::apache::thrift::protocol::TProtocol> input_proto_;
    boost::shared_ptr< ::apache::thrift::protocol::TProtocol> output_proto_;
  };

} } } // namespace

#endif
