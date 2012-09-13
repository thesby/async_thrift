/** @file
 * @brief asynchronous thrift servers
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include <async_server.h>
#include <async_exception.h>
#include <async_connection.h>
#include <set>

namespace apache { namespace thrift { namespace async {

  class AsyncThriftServer::Impl : private boost::noncopyable
  {
    private:
      class Connection;
      class ConnManager;

      class Connection : public AsyncConnection
    {
      private:
        const bool async_rpc_;
        boost::shared_ptr<TProcessor> processor_;
        boost::shared_ptr<AsyncProcessor> async_processor_;
        ConnManager * conn_manager_;

        typedef AsyncConnection BaseType;
      public:

        Connection(boost::asio::io_service& io_service,
            const boost::shared_ptr<TProcessor>& processor);

        Connection(boost::asio::io_service& io_service,
            const boost::shared_ptr<AsyncProcessor>& async_processor);

        virtual ~Connection();

        void set_conn_manager(ConnManager * conn_manager)
        {
          conn_manager_ = conn_manager;
        }

      private:
        void async_process(const boost::system::error_code& ec, bool is_oneway);

      protected:
        virtual void on_close(const boost::system::error_code& ec);
        virtual void on_handle_frame();
    };

      class ConnManager
      {
        private:
          boost::mutex mutex_;
          typedef std::set<Connection *> conn_set;
          conn_set set_;

        public:
          void start(Connection * c);
          void del(Connection * c);
          void del_all();
      };

      void open_acceptor();//may throw
      void async_accept();
      void close_async_accept();
      void handle_accept(const boost::system::error_code& ec);

    private:
      const bool async_rpc_;
      boost::shared_ptr<TProcessor> processor_;
      boost::shared_ptr<AsyncProcessor> async_processor_;

      IOServicePool& io_service_pool_;

      const boost::asio::ip::tcp::endpoint endpoint_;
      boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;

      ConnManager conn_manager_;
      Connection * new_conn_;

    public:
      Impl(
          const boost::shared_ptr<TProcessor>& processor,
          const boost::asio::ip::tcp::endpoint& endpoint,
          IOServicePool& pool)
        :async_rpc_(false),
        processor_(processor),
        io_service_pool_(pool),
        endpoint_(endpoint), new_conn_(0)
    {
    }

      Impl(
          const boost::shared_ptr<AsyncProcessor>& async_processor,
          const boost::asio::ip::tcp::endpoint& endpoint,
          IOServicePool& pool)
        :async_rpc_(true),
        async_processor_(async_processor),
        io_service_pool_(pool),
        endpoint_(endpoint), new_conn_(0)
    {
    }

      ~Impl()
      {
        if (new_conn_)
          delete new_conn_;
      }

      void serve();//may throw
      void stop();
      void stop_impl();

      IOServicePool& get_io_service_pool()
      {
        return io_service_pool_;
      }
  };

  /************************************************************************/
  void AsyncThriftServer::Impl::open_acceptor()
  {
    acceptor_.reset(
        new boost::asio::ip::tcp::acceptor(io_service_pool_.get_io_service()));
    acceptor_->open(endpoint_.protocol());
    boost::asio::socket_base::reuse_address option(true);
    acceptor_->set_option(option);
    acceptor_->bind(endpoint_);//may throw
    acceptor_->listen();
  }

  void AsyncThriftServer::Impl::async_accept()
  {
    if (async_rpc_)
      new_conn_ = new Connection(io_service_pool_.get_io_service(), async_processor_);
    else
      new_conn_ = new Connection(io_service_pool_.get_io_service(), processor_);

    acceptor_->async_accept(new_conn_->get_socket(),
        boost::bind(&AsyncThriftServer::Impl::handle_accept, this, _1));
  }

  void AsyncThriftServer::Impl::close_async_accept()
  {
    boost::system::error_code ec;
    acceptor_->close(ec);
  }

  void AsyncThriftServer::Impl::handle_accept(const boost::system::error_code& ec)
  {
    if (!ec)
    {
      conn_manager_.start(new_conn_);
      new_conn_ = 0;
      async_accept();
    }
    else
    {
      delete new_conn_;
      new_conn_ = 0;
    }
  }

  void AsyncThriftServer::Impl::serve()
  {
    open_acceptor();

    async_accept();

    io_service_pool_.run();
  }

  void AsyncThriftServer::Impl::stop()
  {
    io_service_pool_.get_io_service(0).post(
        boost::bind(&AsyncThriftServer::Impl::stop_impl, this));
  }

  void AsyncThriftServer::Impl::stop_impl()
  {
    close_async_accept();

    io_service_pool_.stop();

    //conn_manager_.del_all(); this may cause core
  }

  /************************************************************************/
  void AsyncThriftServer::Impl::ConnManager::start(AsyncThriftServer::Impl::Connection * c)
  {
    {
      boost::mutex::scoped_lock guard(mutex_);
      set_.insert(c);
    }
    c->set_conn_manager(this);
    c->start_recv(true);
  }

  void AsyncThriftServer::Impl::ConnManager::del(AsyncThriftServer::Impl::Connection * c)
  {
    {
      boost::mutex::scoped_lock guard(mutex_);
      set_.erase(c);
    }
  }

  void AsyncThriftServer::Impl::ConnManager::del_all()
  {
    boost::mutex::scoped_lock guard(mutex_);
    conn_set::iterator first, last;
    first = set_.begin();
    last = set_.end();

    for (; first!=last ; ++first)
    {
      (*first)->set_conn_manager(0);
      delete (*first);
    }

    set_.clear();
  }

  /************************************************************************/
  AsyncThriftServer::Impl::Connection::Connection(boost::asio::io_service& io_service,
      const boost::shared_ptr<TProcessor>& processor)
    :BaseType(io_service), async_rpc_(false),
    processor_(processor),
    conn_manager_(0)
  {
  }

  AsyncThriftServer::Impl::Connection::Connection(boost::asio::io_service& io_service,
      const boost::shared_ptr<AsyncProcessor>& async_processor)
    :BaseType(io_service), async_rpc_(true),
    async_processor_(async_processor),
    conn_manager_(0)
  {
  }

  AsyncThriftServer::Impl::Connection::~Connection()
  {
    if (conn_manager_)
      conn_manager_->del(this);
  }

  void AsyncThriftServer::Impl::Connection::async_process(
      const boost::system::error_code& ec, bool is_oneway)
  {
    assert(async_rpc_);
    if (ec)
    {
      on_close(ec);//current connection diminishes here
      return;
    }

    if (!is_oneway)
      start_write_output_buffer();
    else
      start_recv(false);
  }

  void AsyncThriftServer::Impl::Connection::on_close(const boost::system::error_code& ec)
  {
    BaseType::on_close(ec);

    delete this;//NOTICE!!!
  }

  void AsyncThriftServer::Impl::Connection::on_handle_frame()
  {
    try
    {
      if (async_rpc_)
      {
        async_processor_->process(
            input_proto_, output_proto_,
            boost::bind(&Connection::async_process, this, _1, _2));//may throw
      }
      else
      {
        processor_->process(input_proto_, output_proto_);//may throw
        //NOTICE!!!
        //AsyncThriftServer does not support oneway functions, it always replies here.
        //Because we could not know whether a RPC is oneway.
        //This defect will not be fixed, AsyncThriftServerEx is a substitute.
        start_write_output_buffer();
      }
    }
    catch (TApplicationException& e)
    {
      GlobalOutput.printf("on_handle_frame: %s", e.what());
      boost::system::error_code ec = make_error_code(e);
      on_close(ec);
    }
    catch (TProtocolException& e)
    {
      GlobalOutput.printf("on_handle_frame: %s", e.what());
      boost::system::error_code ec = make_error_code(e);
      on_close(ec);
    }
    catch (TTransportException& e)
    {
      GlobalOutput.printf("on_handle_frame: %s", e.what());
      boost::system::error_code ec = make_error_code(e);
      on_close(ec);
    }
    catch (TException& e)
    {
      GlobalOutput.printf("on_handle_frame: %s", e.what());
      boost::system::error_code ec = make_error_code(e);
      on_close(ec);
    }
    catch (...)
    {
      GlobalOutput.printf("on_handle_frame: error");
      boost::system::error_code ec(
          boost::system::posix_error::bad_message, boost::system::get_posix_category());
      on_close(ec);//current connection diminishes at each on_close
    }
  }

  /************************************************************************/
  AsyncThriftServer::AsyncThriftServer(
      const boost::shared_ptr<TProcessor>& processor,
      const boost::asio::ip::tcp::endpoint& endpoint,
      IOServicePool& pool)
    :TServer(boost::shared_ptr<TProcessor>())
  {
    impl_ = new Impl(processor, endpoint, pool);
  }

  AsyncThriftServer::AsyncThriftServer(
      const boost::shared_ptr<AsyncProcessor>& async_processor,
      const boost::asio::ip::tcp::endpoint& endpoint,
      IOServicePool& pool)
    :TServer(boost::shared_ptr<TProcessor>())
  {
    impl_ = new Impl(async_processor, endpoint, pool);
  }

  AsyncThriftServer::~AsyncThriftServer()
  {
    delete impl_;
  }

  void AsyncThriftServer::serve()
  {
    impl_->serve();
  }

  void AsyncThriftServer::stop()
  {
    impl_->stop();
  }

  IOServicePool& AsyncThriftServer::get_io_service_pool()
  {
    return impl_->get_io_service_pool();
  }

} } } // namespace
