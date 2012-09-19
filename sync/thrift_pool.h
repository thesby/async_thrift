/** @file
 * @brief thrift pool
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef THRIFT_POOL_H
#define THRIFT_POOL_H

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <algorithm>
#include <utility>
#include <set>
#include <map>
#include <vector>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <thrift/Thrift.h>
#include <fb303/FacebookBase.h>
#include <protocol/TBinaryProtocol.h>
#include <transport/TSocket.h>
#include <transport/TBufferTransports.h>

namespace apache { namespace thrift { namespace sync {

  using ::apache::thrift::GlobalOutput;

  enum kTCStatus
  {
    kClosed = 0,
    kConnected,
  };

  template <class T>
    class ThriftConnection
    {
      // T must be a subclass of ::facebook::fb303::FacebookBase
      private:
        const std::string host_;
        const int port_;

        const bool framed_transport_;
        const bool auto_connect_;
        const int thrift_conn_timeout_;
        const int thrift_send_timeout_;
        const int thrift_recv_timeout_;

        mutable boost::recursive_mutex mutex_;// guard the following variants
        kTCStatus status_;
        bool in_use_;
        boost::scoped_ptr<T> client_;


        bool inner_init()
        {
          boost::shared_ptr< ::apache::thrift::transport::TSocket>
            socket(new ::apache::thrift::transport::TSocket(host_, port_));

          socket->setConnTimeout(thrift_conn_timeout_);
          socket->setSendTimeout(thrift_send_timeout_);
          socket->setRecvTimeout(thrift_recv_timeout_);

          boost::shared_ptr< ::apache::thrift::transport::TTransport> transport;
          if (framed_transport_)
            transport.reset(new ::apache::thrift::transport::TFramedTransport(socket));
          else
            transport.reset(new ::apache::thrift::transport::TBufferedTransport(socket));

          boost::shared_ptr< ::apache::thrift::protocol::TBinaryProtocol>
            protocol(new ::apache::thrift::protocol::TBinaryProtocol(transport));

          client_.reset(new T(protocol));

          if (auto_connect_)
            return connect();

          return true;
        }


      public:
        std::string get_host()const
        {
          return host_;
        }

        int get_port()const
        {
          return port_;
        }

        bool get_framed_transport()const
        {
          return framed_transport_;
        }

        bool get_auto_connect()const
        {
          return auto_connect_;
        }

        kTCStatus get_status()const
        {
          return status_;
        }


        // never returns NULL
        T * get_client()const
        {
          return client_.get();
        }


        ThriftConnection(
            const std::string& host, int port,
            bool framed_transport,
            bool auto_connect,
            int thrift_conn_timeout,
            int thrift_send_timeout,
            int thrift_recv_timeout)
          :host_(host), port_(port),
          framed_transport_(framed_transport),
          auto_connect_(auto_connect),
          thrift_conn_timeout_(thrift_conn_timeout),
          thrift_send_timeout_(thrift_send_timeout),
          thrift_recv_timeout_(thrift_recv_timeout),
          mutex_(),
          status_(kClosed),
          in_use_(false),
          client_()
      {
        inner_init();
      }


        ~ThriftConnection()
        {
          // we must put it back before being destroyed
          assert(!in_use_);
          close();
          client_.reset();
        }


        // ever returns NULL
        T * client()
        {
          // connect and get client
          reconnect();
          return get_client();
        }


        bool is_open()const
        {
          boost::recursive_mutex::scoped_lock guard(mutex_);
          return status_ == kConnected
            && client_->getInputProtocol()->getTransport()->isOpen();
        }


        bool connect()
        {
          boost::recursive_mutex::scoped_lock guard(mutex_);
          close();

          try
          {
            client_->getInputProtocol()->getTransport()->open();
          }
          catch (::apache::thrift::TException& e)
          {
            client_->getInputProtocol()->getTransport()->close();
            status_ = kClosed;
            assert(!is_open());
            return false;
          }

          status_ = kConnected;
          assert(is_open());
          return true;
        }


        void reconnect()
        {
          // connect if needed
          boost::recursive_mutex::scoped_lock guard(mutex_);
          if (!is_open())
          {
            connect();
          }
        }


        void close()
        {
          boost::recursive_mutex::scoped_lock guard(mutex_);
          if (status_ == kConnected)
          {
            try
            {
              client_->getInputProtocol()->getTransport()->close();
            }
            catch (...)
            {
            }
            status_ = kClosed;
          }
          else
          {
            assert(status_ == kClosed);
          }

          assert(!is_open());
        }


        // add reference
        bool get()
        {
          boost::recursive_mutex::scoped_lock guard(mutex_);
          if (is_open() && !in_use_)
          {
            in_use_ = true;
            return true;
          }
          else
          {
            return false;
          }
        }


        // release reference
        void put()
        {
          boost::recursive_mutex::scoped_lock guard(mutex_);
          assert(in_use_);
          in_use_ = false;
        }


        bool keep_alive()
        {
          boost::recursive_mutex::scoped_lock guard(mutex_);
          if (!is_open())
            return false;

          bool ret = false;

          try
          {
            ret = (client_->getStatus() == ::facebook::fb303::ALIVE);
          }
          catch (::apache::thrift::TException& e)
          {
            close();
          }

          return ret;
        }
    };


  template <class T>
    class ThriftConnectionPool
    {
      public:
        typedef ThriftConnection<T> ThriftConnectionType;
        typedef ThriftConnectionPool<T> ThriftConnectionPoolType;


      private:
        const std::string host_;
        const int port_;

        const bool framed_transport_;
        const int thrift_conn_timeout_;
        const int thrift_send_timeout_;
        const int thrift_recv_timeout_;
        const int min_free_conn_;// only suggestions
        const int max_conn_;// only suggestions


        mutable boost::recursive_mutex mutex_;
        kTCStatus status_;

        // consider hash_set/unordered_set when connections increase
        typedef std::set<ThriftConnectionType *> ConnectionsType;
        ConnectionsType free_connections_;
        ConnectionsType allocated_connections_;
        ConnectionsType closed_connections_;


      private:
        void inner_init()
        {
          assert(!host_.empty() && port_);

          ThriftConnectionType * client;
          for (int i=0; i<min_free_conn_; ++i)
          {
            client = create_connection();
            if (client)
            {
              if (client->is_open())
                free_connections_.insert(client);
              else
              {
                closed_connections_.insert(client);
                // do not try any more
                break;
              }
            }
          }
        }


        ThriftConnectionType * create_connection()
        {
          ThriftConnectionType * client = NULL;

          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            if (!closed_connections_.empty())
            {
              client = *closed_connections_.begin();
              closed_connections_.erase(client);
            }
          }

          if (client)
            client->connect();
          else
            client = new ThriftConnectionType(host_, port_,
                framed_transport_, true,
                thrift_conn_timeout_,
                thrift_send_timeout_,
                thrift_recv_timeout_);

          assert(client);
          return client;
        }

        void destroy_connection(ThriftConnectionType * client)
        {
          assert(client);
          delete client;
        }

        void put_connection(ThriftConnectionType * client)
        {
          assert(client);
          if (client->is_open())
          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            free_connections_.insert(client);
          }
          else
          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            closed_connections_.insert(client);
          }
        }

      public:
        std::string get_host()const
        {
          return host_;
        }

        int get_port()const
        {
          return port_;
        }

        bool get_framed_transport()const
        {
          return framed_transport_;
        }

        kTCStatus get_status()const
        {
          return status_;
        }

        int get_min_free_conn()const
        {
          return min_free_conn_;
        }

        int get_max_conn()const
        {
          return max_conn_;
        }


        ThriftConnectionPool(const std::string& host, int port,
            bool framed_transport,
            int thrift_conn_timeout,
            int thrift_send_timeout,
            int thrift_recv_timeout,
            int min_free_conn,
            int max_conn)
          :host_(host), port_(port),
          framed_transport_(framed_transport),
          thrift_conn_timeout_(thrift_conn_timeout),
          thrift_send_timeout_(thrift_send_timeout),
          thrift_recv_timeout_(thrift_recv_timeout),
          min_free_conn_(min_free_conn),
          max_conn_(max_conn),
          mutex_(),
          status_(kClosed),
          free_connections_(),
          allocated_connections_(),
          closed_connections_(),

          probe_thread_(),
          io_service_(),
          update_status_timer_(io_service_),
          quick_update_status_timer_(io_service_),
          reclaim_conn_timer_(io_service_),

          got_from_free_conn_(0),
          got_from_created_conn_(0),
          status_connected_(0),
          status_closed_(0),
          reclaim_closed_conn_(0),
          reclaim_free_conn_(0),
          create_free_conn_(0)
          {
            inner_init();
            start_probe();
          }


        ~ThriftConnectionPool()
        {
          stop_probe();

          assert(can_be_destroyed());

          BOOST_FOREACH(ThriftConnectionType * conn, free_connections_)
          {
            destroy_connection(conn);
          }
          free_connections_.clear();

          BOOST_FOREACH(ThriftConnectionType * conn, closed_connections_)
          {
            destroy_connection(conn);
          }
          closed_connections_.clear();
        }


        // we must assure that users put all connections back before being destroyed
        bool can_be_destroyed()const
        {
          boost::recursive_mutex::scoped_lock guard(mutex_);
          return allocated_connections_.empty();
        }


        ThriftConnectionType * get()
        {
          bool from_free = true;
          ThriftConnectionType * client = NULL;

          {
            boost::recursive_mutex::scoped_lock guard(mutex_);

            if (!free_connections_.empty())
            {
              client = *free_connections_.begin();
              assert(client);
              free_connections_.erase(client);
            }
          }

          // no enough free connections, we create one
          if (client == NULL)
          {
            client = create_connection();
            from_free = false;
          }
          if (client == NULL)
            return NULL;

          if (client->get())
          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            allocated_connections_.insert(client);

            if (from_free)
              got_from_free_conn_++;
            else
              got_from_created_conn_++;
            return client;
          }

          put_connection(client);
          return NULL;
        }


        void put(ThriftConnectionType * client)
        {
          if (client == NULL)
            return;

          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            allocated_connections_.erase(client);
          }

          client->put();
          put_connection(client);
        }


        // we seldom 'close' the client, except that the connection is abnormal.
        // so, every time 'client' is to be 'close',
        // we immediately check the remote server by 'set_quick_update_status'
        void close(ThriftConnectionType * client)
        {
          if (client == NULL)
            return;

          client->close();

          set_quick_update_status();
        }


        void get_options(std::map<std::string, std::string>& options)const
        {
          size_t as, fs, cs;
          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            as = allocated_connections_.size();
            fs = free_connections_.size();
            cs = closed_connections_.size();
          }

          options.insert(std::make_pair(
                str(boost::format("%s:%s status") % host_ % port_),
                str(boost::format("%u") % status_)));
          options.insert(std::make_pair(
                str(boost::format("%s:%s allocated connections") % host_ % port_),
                str(boost::format("%lu") % as)));
          options.insert(std::make_pair(
                str(boost::format("%s:%s free connections") % host_ % port_),
                str(boost::format("%lu") % fs)));
          options.insert(std::make_pair(
                str(boost::format("%s:%s closed connections") % host_ % port_),
                str(boost::format("%lu") % cs)));

          options.insert(std::make_pair(
                str(boost::format("%s:%s got from free connections") % host_ % port_),
                str(boost::format("%lu") % got_from_free_conn_)));
          options.insert(std::make_pair(
                str(boost::format("%s:%s got from created connections") % host_ % port_),
                str(boost::format("%lu") % got_from_created_conn_)));
          options.insert(std::make_pair(
                str(boost::format("%s:%s status connected") % host_ % port_),
                str(boost::format("%lu") % status_connected_)));
          options.insert(std::make_pair(
                str(boost::format("%s:%s status closed") % host_ % port_),
                str(boost::format("%lu") % status_closed_)));
          options.insert(std::make_pair(
                str(boost::format("%s:%s reclaim closed connections") % host_ % port_),
                str(boost::format("%lu") % reclaim_closed_conn_)));
          options.insert(std::make_pair(
                str(boost::format("%s:%s reclaim free connections") % host_ % port_),
                str(boost::format("%lu") % reclaim_free_conn_)));
          options.insert(std::make_pair(
                str(boost::format("%s:%s create free connections") % host_ % port_),
                str(boost::format("%lu") % create_free_conn_)));
        }


        void get_stat(std::string& stat)const
        {
          size_t as, fs, cs;
          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            as = allocated_connections_.size();
            fs = free_connections_.size();
            cs = closed_connections_.size();
          }

          stat = str(boost::format("(%s:%s:%u|%lu|%lu|%lu)")
              % host_ % port_ % status_
              % as % fs % cs
              );
        }


      private:
        // about probe threads
        boost::scoped_ptr<boost::thread> probe_thread_;
        boost::asio::io_service io_service_;
        boost::asio::deadline_timer update_status_timer_;
        boost::asio::deadline_timer quick_update_status_timer_;
        boost::asio::deadline_timer reclaim_conn_timer_;


        void start_probe()
        {
          if (!probe_thread_)
          {
            set_update_status_immediate();
            set_reclaim_connections();

            io_service_.reset();
            probe_thread_.reset(new boost::thread
                (boost::bind(&boost::asio::io_service::run, &io_service_)));
          }
        }


        void stop_probe()
        {
          if (probe_thread_)
          {
            io_service_.stop();
            probe_thread_->join();
            probe_thread_.reset();
          }
        }


        void set_update_status_immediate()
        {
          update_status_timer_.expires_from_now(boost::posix_time::seconds(0));
          update_status_timer_.async_wait(boost::bind(&ThriftConnectionPool::update_status, this));
        }


        void set_update_status()
        {
          update_status_timer_.expires_from_now(boost::posix_time::seconds(1));
          update_status_timer_.async_wait(boost::bind(&ThriftConnectionPool::update_status, this));
        }


        void set_quick_update_status()
        {
          update_status_timer_.expires_from_now(boost::posix_time::seconds(0));
          update_status_timer_.async_wait(boost::bind(&ThriftConnectionPool::quick_update_status, this));
        }


        void set_reclaim_connections()
        {
          reclaim_conn_timer_.expires_from_now(boost::posix_time::seconds(10));
          reclaim_conn_timer_.async_wait(boost::bind(&ThriftConnectionPool::reclaim_connections, this));
        }


        void update_status()
        {
          // get/create one free connection
          ThriftConnectionType * client = NULL;
          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            if (!free_connections_.empty())
            {
              client = *free_connections_.begin();
              assert(client);
              free_connections_.erase(client);
            }
          }
          if (client == NULL)
            client = create_connection();


          kTCStatus prev_status;
          kTCStatus new_status;
          int try_times = 3;// hard code
          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            prev_status = status_;
            if (status_ == kClosed)
              try_times = 1;
          }

          // if one good free connection getStatus fails over 3 times, mark status_ kClosed
          // if ever success, mark status_ kConnected
          bool ok = false;
          for (int i=0; i<try_times; i++)
          {
            if (client->is_open())
            {
              T * cli = client->get_client();
              ::facebook::fb303::fb_status status = ::facebook::fb303::DEAD;
              try
              {
                status = cli->getStatus();
                if (status == ::facebook::fb303::ALIVE)
                {
                  ok = true;
                  break;
                }
              }
              catch (::apache::thrift::TException& e)
              {
                client->close();
              }
            }
          }

          put_connection(client);

          new_status = ok ? kConnected : kClosed;

          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            status_ = new_status;

            if (ok)
              status_connected_++;
            else
              status_closed_++;
          }

          set_update_status();
        }


        void quick_update_status()
        {
          // get/create one free connection
          ThriftConnectionType * client = NULL;
          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            if (!free_connections_.empty())
            {
              client = *free_connections_.begin();
              assert(client);
              free_connections_.erase(client);
            }
          }
          if (client == NULL)
            client = create_connection();


          kTCStatus new_status;
          bool ok = false;

          if (client->is_open())
          {
            T * cli = client->get_client();
            ::facebook::fb303::fb_status status = ::facebook::fb303::DEAD;
            try
            {
              status = cli->getStatus();
              if (status == ::facebook::fb303::ALIVE)
              {
                ok = true;
              }
            }
            catch (::apache::thrift::TException& e)
            {
              client->close();
            }
          }

          put_connection(client);

          new_status = ok ? kConnected : kClosed;

          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            status_ = new_status;

            if (ok)
              status_connected_++;
            else
              status_closed_++;
          }
        }


        void reclaim_connections()
        {
          do
          {
            size_t as, fs, cs, ts;

            // free connections by: min_free_conn_ and max_conn_
            {
              boost::recursive_mutex::scoped_lock guard(mutex_);
              as = allocated_connections_.size();
              fs = free_connections_.size();
              cs = closed_connections_.size();
              ts = as + fs + cs;
            }

            if (ts < static_cast<size_t>(max_conn_))
            {
              break;
            }

            size_t to_be_freed = ts - static_cast<size_t>(max_conn_);
            size_t cs_to_be_freed;

            if (to_be_freed > cs)
              cs_to_be_freed = cs;
            else
              cs_to_be_freed = to_be_freed;
            to_be_freed -= cs_to_be_freed;


            if (fs <= static_cast<size_t>(min_free_conn_))
            {
              size_t fs_to_be_created;
              fs_to_be_created = min_free_conn_ - fs;

              if (fs_to_be_created)
              {
                create_free_conn_++;
              }

              for (size_t i=0; i<fs_to_be_created; i++)
              {
                ThriftConnectionType * client = create_connection();
                if (client)
                {
                  put_connection(client);
                }
              }
            }
            else
            {
              size_t fs_to_be_freed;
              if (to_be_freed > fs - min_free_conn_)
                to_be_freed = fs - min_free_conn_;
              fs_to_be_freed = to_be_freed;

              if (fs_to_be_freed)
              {
                reclaim_free_conn_++;
                GlobalOutput.printf("ThriftConnectionPool::reclaim_connections() <%s:%d> free connection is excessive, reclaim %u",
                    get_host().c_str(), get_port(),
                    static_cast<uint32_t>(fs_to_be_freed));
              }

              for (size_t i=0; i<fs_to_be_freed; i++)
              {
                ThriftConnectionType * client = NULL;
                {
                  boost::recursive_mutex::scoped_lock guard(mutex_);
                  if (free_connections_.empty())
                    break;
                  client = *free_connections_.begin();
                  assert(client);
                  free_connections_.erase(client);
                }
                if (client)
                  destroy_connection(client);
              }
            }


            if (cs_to_be_freed)
            {
              reclaim_closed_conn_++;
              GlobalOutput.printf("ThriftConnectionPool::reclaim_connections() <%s:%d> closed connection is excessive, reclaim %u",
                  get_host().c_str(), get_port(),
                  static_cast<uint32_t>(cs_to_be_freed));
            }

            for (size_t i=0; i<cs_to_be_freed; i++)
            {
              ThriftConnectionType * client = NULL;
              {
                boost::recursive_mutex::scoped_lock guard(mutex_);
                if (closed_connections_.empty())
                  break;
                client = *closed_connections_.begin();
                assert(client);
                closed_connections_.erase(client);
              }
              if (client)
                destroy_connection(client);
            }
          }while(0);


          set_reclaim_connections();
        }

      private:
        // about statistics

        // need guard(multi write, multi read)
        uint64_t got_from_free_conn_;
        uint64_t got_from_created_conn_;
        uint64_t status_connected_;
        uint64_t status_closed_;

        // need not guard(single write, multi read)
        uint64_t reclaim_closed_conn_;
        uint64_t reclaim_free_conn_;
        uint64_t create_free_conn_;
    };


  template <class T>
    class ThriftPool
    {
      public:
        typedef ThriftConnection<T> ThriftConnectionType;
        typedef ThriftConnectionPool<T> ThriftConnectionPoolType;
        typedef ThriftPool<T> ThriftPoolType;

      protected:
        typedef std::map<ThriftConnectionType *, ThriftConnectionPoolType *> ConnPoolMap;
        typedef std::vector<ThriftConnectionPoolType *> PoolsType;

        const bool framed_transport_;
        const int thrift_conn_timeout_;
        const int thrift_send_timeout_;
        const int thrift_recv_timeout_;
        const int min_free_conn_;// only suggestions
        const int max_conn_;// only suggestions
        const std::string localhost_name_;

      private:
        mutable boost::recursive_mutex mutex_;
        int total_conn_;
        size_t index_;
        ConnPoolMap conn_pool_map_;
        PoolsType pools_;
        // local host
        ThriftConnectionPoolType * localhost_pool_;


        // for async 'remove_server'
        // we can not remove a ThriftConnectionPool of host if any client holds some connections.
        // instead, remove_queue_ is a temporary place to accommodate the pools
        // until all connections are put back
        mutable boost::recursive_mutex remove_queue_mutex_;
        bool remove_thread_stop_flag_;
        boost::scoped_ptr<boost::thread> remove_thread_;
        PoolsType remove_queue_;

      private:
        void start_remove_thread()
        {
          if (!remove_thread_)
          {
            remove_thread_stop_flag_ = false;
            remove_thread_.reset(new boost::thread
                (boost::bind(&ThriftPool::remove_thread, this)));
          }
        }


        void stop_remove_thread()
        {
          if (remove_thread_)
          {
            remove_thread_stop_flag_ = true;
            remove_thread_->join();
            remove_thread_.reset();
          }
        }


        void remove_thread()
        {
          for (;;)
          {
            {
              boost::recursive_mutex::scoped_lock guard(remove_queue_mutex_);

              typename PoolsType::iterator first = remove_queue_.begin();
              for (;first != remove_queue_.end();)
              {
                ThriftConnectionPoolType * pool = (*first);
                assert(pool);
                if (pool->can_be_destroyed())
                {
                  first = remove_queue_.erase(first);
                  GlobalOutput.printf("ThriftPool::remove_thread() remove host <%s:%d>",
                      pool->get_host().c_str(), pool->get_port());
                  delete pool;
                }
                else
                {
                  ++first;
                }
              }

              if (!remove_thread_stop_flag_ && remove_queue_.empty())
              {
                GlobalOutput("ThriftPool::remove_thread() wait until all connections being put back");
                break;
              }
            }
            boost::this_thread::sleep(boost::posix_time::seconds(1));
          }
        }


      public:
        ThriftPool(
            bool framed_transport = true,
            int thrift_conn_timeout = 50,
            int thrift_send_timeout = 50,
            int thrift_recv_timeout = 50,
            int min_free_conn = 10,
            int max_conn = 100)
          :framed_transport_(framed_transport),
          thrift_conn_timeout_(thrift_conn_timeout),
          thrift_send_timeout_(thrift_send_timeout),
          thrift_recv_timeout_(thrift_recv_timeout),
          min_free_conn_(min_free_conn),
          max_conn_(max_conn),
          localhost_name_(boost::asio::ip::host_name()),
          total_conn_(0),
          index_(0),
          localhost_pool_(NULL),
          remove_thread_stop_flag_(false)
      {
        start_remove_thread();
      }


        ThriftPool(
            const std::string& backends,
            bool framed_transport = true,
            int thrift_conn_timeout = 50,
            int thrift_send_timeout = 50,
            int thrift_recv_timeout = 50,
            int min_free_conn = 10,
            int max_conn = 100)
          :framed_transport_(framed_transport),
          thrift_conn_timeout_(thrift_conn_timeout),
          thrift_send_timeout_(thrift_send_timeout),
          thrift_recv_timeout_(thrift_recv_timeout),
          min_free_conn_(min_free_conn),
          max_conn_(max_conn),
          localhost_name_(boost::asio::ip::host_name()),
          total_conn_(0),
          index_(0),
          localhost_pool_(NULL),
          remove_thread_stop_flag_(false)
      {
        std::vector<std::string> split_vector;
        (void)boost::split(split_vector, backends, boost::is_any_of(","));
        for (size_t i=0; i<split_vector.size(); ++i)
        {
          std::vector<std::string> host_port;
          (void)boost::split(host_port, split_vector[i], boost::is_any_of(":"));

          try
          {
            if (host_port.size() == 2)
              add_server(host_port[0], boost::lexical_cast<int>(host_port[1]));
          }
          catch (std::exception& e)
          {
            GlobalOutput.printf("ThriftPool::ThriftPool() caught a std::exception: %s", e.what());
          }
          catch (...)
          {
            GlobalOutput("ThriftPool::ThriftPool() caught an unknown exception");
          }
        }

        start_remove_thread();
      }


        virtual ~ThriftPool()
        {
          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            BOOST_FOREACH(ThriftConnectionPoolType * pool, pools_)
            {
              assert(pool);
              boost::recursive_mutex::scoped_lock guard(remove_queue_mutex_);
              remove_queue_.push_back(pool);
            }
            pools_.clear();
          }

          stop_remove_thread();
        }


        void set_total_conn(int total_conn)
        {
          boost::recursive_mutex::scoped_lock guard(mutex_);

          total_conn_ = total_conn;

          if (total_conn_ != 0)
          {
            int max_conn = total_conn_ / pools_.size() + 1;
            BOOST_FOREACH(ThriftConnectionPoolType * pool, pools_)
            {
              assert(pool);
              pool->set_max_conn(max_conn);
              pool->set_min_free_conn(std::max(max_conn/10, min_free_conn_));
            }
          }

          if (localhost_pool_)
          {
            localhost_pool_->set_max_conn(total_conn);
            localhost_pool_->set_min_free_conn(std::max(total_conn/10, min_free_conn_));
          }
        }


        int get_total_conn()const
        {
          return total_conn_;
        }


        bool update_servers(const std::vector<std::string>& hosts, const std::vector<int>& ports)
        {
          if (hosts.size() != ports.size())
            return false;

          std::vector<bool> hosts_found(hosts.size(), false);


          {
            typename PoolsType::iterator first;

            // lock both of them
            boost::recursive_mutex::scoped_lock guard1(mutex_);
            boost::recursive_mutex::scoped_lock guard2(remove_queue_mutex_);

            // update pools_
            first = pools_.begin();

            for (; first != pools_.end(); )
            {
              bool found = false;
              ThriftConnectionPoolType * pool = (*first);
              assert(pool);

              for (size_t j=0; j<hosts.size(); j++)
              {
                if (pool->get_host() == hosts[j] && pool->get_port() == ports[j]
                    && !hosts_found[j])
                {
                  hosts_found[j] = true;
                  found = true;
                  break;
                }
              }

              if (found)
              {
                // found pool in hosts, do nothing
                ++first;
              }
              else
              {
                // not found pool in hosts, erase it
                if (localhost_name_ == pool->get_host())
                  localhost_pool_ = NULL;
                first = pools_.erase(first);
                remove_queue_.push_back(pool);
              }
            }

            // update 'remove_queue_'
            first = remove_queue_.begin();

            for (; first != remove_queue_.end(); )
            {
              bool found = false;
              ThriftConnectionPoolType * pool = (*first);
              assert(pool);

              for (size_t j=0; j<hosts.size(); j++)
              {
                if (pool->get_host() == hosts[j] && pool->get_port() == ports[j]
                    && !hosts_found[j])
                {
                  hosts_found[j] = true;
                  found = true;
                  break;
                }
              }

              if (found)
              {
                // found pool in hosts, put the 'pool' from 'remove_queue_' back to 'pools_'
                first = remove_queue_.erase(first);
                pools_.push_back(pool);

                if (localhost_name_ == pool->get_host())
                  localhost_pool_ = pool;
              }
              else
              {
                // not found pool in hosts, do nothing
                ++first;
              }
            }

            // add all remaining hosts to 'pools_'
            for (size_t j=0; j<hosts.size(); j++)
            {
              if (!hosts_found[j])
              {
                ThriftConnectionPoolType * pool = new ThriftConnectionPoolType(
                    hosts[j],
                    ports[j],
                    framed_transport_,
                    thrift_conn_timeout_,
                    thrift_send_timeout_,
                    thrift_recv_timeout_,
                    min_free_conn_,
                    max_conn_);

                pools_.push_back(pool);

                if (localhost_name_ == hosts[j])
                  localhost_pool_ = pool;
              }
            }
          }

          return true;
        }


        bool add_server(const std::string& host, int port)
        {
          assert(!host.empty() && port);

          boost::recursive_mutex::scoped_lock guard(mutex_);

          BOOST_FOREACH(const ThriftConnectionPoolType * pool, pools_)
          {
            assert(pool);
            if (pool->get_host() == host && pool->get_port() == port)
            {
              return false;
            }
          }

          ThriftConnectionPoolType * pool = new ThriftConnectionPoolType(
              host,
              port,
              framed_transport_,
              thrift_conn_timeout_,
              thrift_send_timeout_,
              thrift_recv_timeout_,
              min_free_conn_,
              max_conn_);

          pools_.push_back(pool);

          if (localhost_name_ == host)
            localhost_pool_ = pool;

          return true;
        }


        void remove_server(const std::string& host, int port)
        {
          ThriftConnectionPoolType * pool = NULL;

          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            typename PoolsType::iterator first = pools_.begin();
            typename PoolsType::iterator last = pools_.end();

            for (; first != last; ++first)
            {
              if ((*first)->get_host() == host && (*first)->get_port() == port)
              {
                pool = (*first);
                if (localhost_name_ == host)
                  localhost_pool_ = NULL;
                pools_.erase(first);
                break;
              }
            }
          }

          if (pool)
          {
            boost::recursive_mutex::scoped_lock guard(remove_queue_mutex_);
            remove_queue_.push_back(pool);

            // re-schedule total connections
            set_total_conn(total_conn_);
          }
        }


        virtual ThriftConnectionType * get()
        {
          ThriftConnectionType * client;
          ThriftConnectionPoolType * pool = NULL;

          boost::recursive_mutex::scoped_lock guard(mutex_);
          if (localhost_pool_ && localhost_pool_->get_status() == kConnected)
          {
            pool = localhost_pool_;
          }
          else
          {
            size_t servers_size = pools_.size();
            if (servers_size == 0)
            {
              return NULL;
            }

            for (size_t i=0; i<servers_size; i++)
            {
              index_ = index_ % servers_size;
              pool = pools_[index_++];
              assert(pool);

              if (pool->get_status() == kConnected)
                break;
            }

            if (pool->get_status() != kConnected)
            {
              GlobalOutput.printf("ThriftPool::get() <%s:%d> no available hosts",
                  pool->get_host().c_str(), pool->get_port());
              return NULL;
            }
          }

          client = pool->get();
          if (client == NULL)
          {
            GlobalOutput.printf("ThriftPool::get() <%s:%d> get connection fail",
                pool->get_host().c_str(), pool->get_port());
            return NULL;
          }

          conn_pool_map_[client] = pool;
          return client;
        }


        virtual bool put(ThriftConnectionType * client)
        {
          if (client == NULL)
            return false;

          boost::recursive_mutex::scoped_lock guard(mutex_);

          typename ConnPoolMap::iterator it = conn_pool_map_.find(client);
          if (it != conn_pool_map_.end())
          {
            it->second->put(client);
            conn_pool_map_.erase(client);
            return true;
          }

          return false;
        }


        virtual bool close(ThriftConnectionType * client)
        {
          if (client == NULL)
            return false;

          boost::recursive_mutex::scoped_lock guard(mutex_);
          typename ConnPoolMap::iterator it = conn_pool_map_.find(client);
          if (it != conn_pool_map_.end())
          {
            it->second->close(client);
            return true;
          }

          return false;
        }


        kTCStatus get_status()const
        {
          boost::recursive_mutex::scoped_lock guard(mutex_);
          BOOST_FOREACH(const ThriftConnectionPoolType * pool, pools_)
          {
            assert(pool);
            if (pool->get_status() == kConnected)
              return kConnected;
          }
          return kClosed;
        }


        // 0-100, get the total health degree
        size_t get_health()const
        {
          size_t total;
          size_t connected = 0;

          boost::recursive_mutex::scoped_lock guard(mutex_);
          total = pools_.size();
          BOOST_FOREACH(const ThriftConnectionPoolType * pool, pools_)
          {
            assert(pool);
            if (pool->get_status() == kConnected)
              connected++;
          }

          return connected * 100 / total;
        }


        void get_options(std::map<std::string, std::string>& options)const
        {
          boost::recursive_mutex::scoped_lock guard(mutex_);
          BOOST_FOREACH(const ThriftConnectionPoolType * pool, pools_)
          {
            assert(pool);
            pool->get_options(options);
          }
        }


        void get_stat(std::string& stat)const
        {
          std::vector<std::string> tmps;

          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            tmps.resize(pools_.size());

            for (size_t i=0; i<pools_.size(); i++)
            {
              assert(pools_[i]);
              pools_[i]->get_stat(tmps[i]);
            }
          }

          stat.clear();
          stat += "[";
          for (size_t i=0; i<tmps.size(); i++)
          {
            stat += tmps[i];
          }
          stat += "]";
        }
    };

} } }

#endif
