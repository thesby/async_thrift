/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_THRIFT_CLIENT_H
#define ASYNC_THRIFT_CLIENT_H

#include <list>
#include "AsyncConnection.h"

namespace apache { namespace thrift { namespace async {

  /*
  * AsyncThriftClient(using TFramedTransport, TBinaryProtocol)
  *
  * socket layer asynchronous
  * RPC asynchronous
  *
  * Single Thread!!!
  */
  class AsyncThriftClient : public AsyncConnection
  {
  public:
    typedef boost::function<void (const boost::system::error_code& ec)> AsyncRPCCallback;

  protected:
    boost::shared_ptr<boost::asio::deadline_timer> timer_;
    size_t timeout_;

    struct AsyncOp
    {
      AsyncRPCCallback callback;
      int rpc_type;
      void * _return;//pointer to various type. if it is NULL, return type is "void"
      bool is_oneway;
    };
    std::list<boost::shared_ptr<AsyncOp> > async_op_list_;
    boost::shared_ptr<AsyncOp> pending_async_op_;

  public:
    AsyncThriftClient();
    //socket must be opened
    //this kind of constructor may help users adapt a connection pool with this class
    explicit AsyncThriftClient(const boost::shared_ptr<boost::asio::ip::tcp::socket> socket);
    virtual ~AsyncThriftClient();

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

  protected:
    void set_rpc_timer();
    void cancel_rpc_timer();
    void handle_timeout(const boost::system::error_code& ec);

    virtual void on_close(const boost::system::error_code * ec);
    virtual void on_attach(const boost::shared_ptr<boost::asio::ip::tcp::socket> socket);
    virtual void on_detach();
    virtual void on_handle_read(const boost::system::error_code& ec, size_t bytes_transferred);
    virtual void on_handle_write(const boost::system::error_code& ec, size_t bytes_transferred);
    virtual void on_handle_frame();

    virtual void fill_result(AsyncOp& op) = 0;
  };

} } } // namespace

#endif
