/** @file
* @brief asio tcp socket pool
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <asio_pool.h>
#include <map>

namespace apache { namespace thrift { namespace async {

  class AsioPool::Impl
  {
  private:
    static std::string endpoint_to_string(const EndPoint& endpoint)
    {
      std::ostringstream oss;
      oss << endpoint;
      return oss.str();
    }

    // 检测远程socket是否已断开
    // 注意:不使用fb303.getStatus,而是使用更底层的recv系统调用检测连接,更通用高效
    // 问题:所有的服务必须做好所有准备工作才能监听端口,因为端口打开,则AsioPool认为该服务可用
    static bool socket_is_closed(int sockfd)
    {
      char c;
      int bytes;

      bytes = recv(sockfd, &c, 1, MSG_PEEK | MSG_DONTWAIT);
      if (bytes == 0 || (bytes == -1 && errno != EAGAIN && errno != EINTR))
        return true;

      return false;
    }

    static inline bool socket_is_closed(const SocketSP& sp)
    {
      assert(sp);
      return sp->is_open() && socket_is_closed(sp->native());
    }

    static SocketSP socket_connect(boost::asio::io_service& io_service,
      const EndPoint& endpoint,
      size_t timeout_ms)
    {
      boost::system::error_code ec;
      SocketSP socket_sp;
      socket_sp.reset(new SocketSP::value_type(io_service));

      socket_sp->open(endpoint.protocol());

      int sockfd = socket_sp->native();
      {
        // 设置SO_SNDTIMEO,它将作用于之后的connect操作
        struct timeval timeout = {0, timeout_ms*1000};
        ::setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
      }

      // 带超时的阻塞连接
      socket_sp->connect(endpoint, ec);

      if (ec)
      {
        socket_sp.reset();
        GlobalOutput.printf("connect %s failed: %s\n",
          endpoint_to_string(endpoint).c_str(), ec.message().c_str());
      }
      else
      {
        // 取消SO_SNDTIMEO
        struct timeval timeout = {0, 0};
        ::setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
      }
      return socket_sp;
    }

  private:
    /************************************************************************/
    typedef std::vector<SocketSP> SocketSPVector;

    enum kStatus
    {
      kAdding = 0,// 新添加
      kConnected,// 连接保持,只有此状态的池才可用
      kDisConnected,// 连接断开
      kDeleting,// 正删除
      kDeleted,// 已删除
    };

    /**
    * 状态转移逻辑:
    *                kAdding  kConnected  kDisConnected  kDeleting  kDeleted
    * kAdding           -          O            O           del        X
    * kConnected        X          -            O           del        X
    * kDisConnected     X          O            -           del        X
    * kDeleting        add         X            X            -         O
    * kDeleted         add         X            X            X         -
    */

    static const char * status_to_cstring(kStatus status)
    {
      static const char * str[] =
      {
        "kAdding",
        "kConnected",
        "kDisConnected",
        "kDeleting",
        "kDeleted",
      };
      return str[status];
    }

    static void clear_pool(SocketSPVector * pool)
    {
      size_t size = pool->size();

      for (size_t i=0; i<size; i++)
      {
        SocketSP& socket_sp = (*pool)[i];
        if (socket_sp)
        {
          socket_sp->close();
          socket_sp.reset();
        }
      }

      pool->clear();
    }

    struct EndPointPool
    {
      EndPointPool()
        :status(kDeleted)
      {}

      void close_all()
      {
        clear_pool(&pool);
      }

      kStatus status;// 状态
      SocketSPVector pool;// 池
    };

    typedef std::map<EndPoint, EndPointPool> EndPointPoolMap;

    IOServicePool& ios_pool_;
    const size_t max_conn_per_endpoint_;
    const size_t min_conn_per_endpoint_;
    const size_t connect_timeout_;
    const size_t probe_cycle_;

    mutable boost::recursive_mutex mutex_;// 保护map_,stats_
    EndPointPoolMap map_;

    // 统计信息
    struct Stats
    {
      Stats()
      {
        clear();
      }

      std::string to_string()const
      {
        std::ostringstream oss;
        oss << "got_conn_failure_=" << got_conn_failure_ << std::endl;
        oss << "got_from_pool_conn_=" << got_from_pool_conn_ << std::endl;
        oss << "got_from_created_conn_=" << got_from_created_conn_ << std::endl;
        oss << "connected_count_=" << connected_count_ << std::endl;
        oss << "disconnected_count_=" << disconnected_count_ << std::endl;
        return oss.str();
      }

      void clear()
      {
        got_conn_failure_ = 0;
        got_from_pool_conn_ = 0;
        got_from_created_conn_ = 0;
        connected_count_ = 0;
        disconnected_count_ = 0;
      }

      uint64_t got_conn_failure_;// 获得连接失败的次数
      uint64_t got_from_pool_conn_;// 从池中获得连接的次数
      uint64_t got_from_created_conn_;// 从新建连接获得连接的次数
      uint64_t connected_count_;// 池变为kConnected的次数
      uint64_t disconnected_count_;// 池变为kDisConnected的次数
    };
    Stats stats_;

    // 后台维护连接状态的线程及其相关变量
    boost::scoped_ptr<boost::thread> probe_thread_;
    boost::asio::io_service probe_io_service_;
    boost::asio::deadline_timer probe_timer_;
    boost::asio::deadline_timer quick_probe_timer_;

  private:

    void start_probe()
    {
      if (!probe_thread_)
      {
        set_probe_immediate();

        probe_io_service_.reset();
        probe_thread_.reset(new boost::thread
          (boost::bind(&boost::asio::io_service::run, &probe_io_service_)));
      }
    }

    void stop_probe()
    {
      if (probe_thread_)
      {
        probe_io_service_.stop();
        probe_thread_->join();
        probe_thread_.reset();
      }
    }

    void set_probe_immediate()
    {
      probe_timer_.expires_from_now(boost::posix_time::seconds(0));
      probe_timer_.async_wait(boost::bind(&AsioPool::Impl::probe, this));
    }

    void set_probe()
    {
      probe_timer_.expires_from_now(boost::posix_time::seconds(probe_cycle_));
      probe_timer_.async_wait(boost::bind(&AsioPool::Impl::probe, this));
    }

    void set_quick_probe()
    {
      quick_probe_timer_.expires_from_now(boost::posix_time::seconds(0));
      quick_probe_timer_.async_wait(boost::bind(&AsioPool::Impl::quick_probe, this));
    }

    void __probe(bool quick)
    {
      EndPointPoolMap probe_map;// 需要检测状态的池
      std::vector<SocketSPVector> tmp_pools;
      size_t size;

      {
        // 1.遍历map_,更新状态
        boost::recursive_mutex::scoped_lock guard(mutex_);

        EndPointPoolMap::iterator first = map_.begin();
        EndPointPoolMap::iterator last = map_.end();
        for (; first!=last; ++first)
        {
          const EndPoint& endpoint = (*first).first;
          EndPointPool& endpoint_pool = (*first).second;
          kStatus& status = endpoint_pool.status;
          SocketSPVector& pool = endpoint_pool.pool;

          switch (status)
          {
          case kConnected:
            if (quick)
              break;

            if (!pool.empty())
            {
              if (socket_is_closed(pool.back()))
              {
                // 连接异常断开,关闭同一个池的所有连接
                tmp_pools.resize(tmp_pools.size() + 1);
                endpoint_pool.pool.swap(tmp_pools.back());

                GlobalOutput.printf("%s[kConnected] seems a disconnection\n",
                  endpoint_to_string(endpoint).c_str());
                // fall through
              }
              else
              {
                // 池正常
                GlobalOutput.printf("%s[kConnected] ok\n",
                  endpoint_to_string(endpoint).c_str());
                break;
              }
            }
            else
            {
              // 池为空
              GlobalOutput.printf("%s[kConnected]'s pool is empty\n",
                endpoint_to_string(endpoint).c_str());
              // fall through
            }

          case kAdding:
          case kDisConnected:
            // 加入probe_map,记录status,这里status只可能是kAdding,kConnected,kDisConnected
            // 由下一阶段处理
            probe_map[endpoint].status = status;

            GlobalOutput.printf("%s[%s]\n",
              endpoint_to_string(endpoint).c_str(),
              status_to_cstring(status));
            break;

          case kDeleting:
            status = kDeleted;
            tmp_pools.resize(tmp_pools.size() + 1);
            endpoint_pool.pool.swap(tmp_pools.back());

            GlobalOutput.printf("%s[kDeleting] became [kDeleted]\n",
              endpoint_to_string(endpoint).c_str());
            break;

          case kDeleted:
            // 删除的也保存在map_中,因为很可能将来会继续添加
            break;

          default:
            assert(0);
            break;
          }
        }
      }

      size = tmp_pools.size();
      for (size_t i=0; i<size; i++)
        clear_pool(&tmp_pools[i]);

      // 2.对probe_map中的连接进行检测
      // 如果连接成功则设置kConnected状态,否则设置kDisConnected状态
      {
        EndPointPoolMap::iterator first = probe_map.begin();
        EndPointPoolMap::iterator last = probe_map.end();
        for (; first!=last; ++first)
        {
          const EndPoint& endpoint = (*first).first;
          EndPointPool& endpoint_pool = (*first).second;
          kStatus& status = endpoint_pool.status;

          kStatus new_status;
          {
            boost::recursive_mutex::scoped_lock guard(mutex_);
            new_status = map_[endpoint].status;
          }

          if (status != new_status)
          {
            // 该池的状态被改变了(只可能是调用了del),本轮不处理,交给下一轮第一阶段处理
            assert(new_status == kDeleting);
            GlobalOutput.printf("%s[%s] became [%s]\n",
              endpoint_to_string(endpoint).c_str(),
              status_to_cstring(status),
              status_to_cstring(new_status));
            continue;
          }

          switch (status)
          {
          case kAdding:
          case kConnected:
          case kDisConnected:
            {
              SocketSP socket_sp = socket_connect(ios_pool_.get_io_service(), endpoint, connect_timeout_);
              if (!socket_sp)
              {
                // 连接失败
                if (status == kConnected)
                {
                  stats_.disconnected_count_++;
                  GlobalOutput.printf("%s[kConnected] became [kDisConnected]\n",
                    endpoint_to_string(endpoint).c_str());
                }
                status = kDisConnected;

                {
                  boost::recursive_mutex::scoped_lock guard(mutex_);
                  EndPointPool& new_endpoint_pool = map_[endpoint];
                  if (new_endpoint_pool.status == kDeleting)
                    break;

                  new_endpoint_pool.status = kDisConnected;

                  SocketSPVector tmp_pool;
                  new_endpoint_pool.pool.swap(tmp_pool);

                  // 解锁
                  guard.unlock();
                  // 关闭socket操作可能会阻塞,将它放在锁外面,下同
                  clear_pool(&tmp_pool);
                }
              }
              else
              {
                // 连接成功
                if (status != kConnected)
                {
                  stats_.connected_count_++;
                  GlobalOutput.printf("%s[%s] became [kConnected]\n",
                    endpoint_to_string(endpoint).c_str(),
                    status_to_cstring(status));
                }
                status = kConnected;
                GlobalOutput.printf("%s[kConnected] connection ok\n",
                  endpoint_to_string(endpoint).c_str());

                size_t need_conn = 0;
                {
                  boost::recursive_mutex::scoped_lock guard(mutex_);
                  EndPointPool& new_endpoint_pool = map_[endpoint];
                  if (new_endpoint_pool.status == kDeleting)
                    // 该池的状态被改变了(只可能是调用了del)
                    break;

                  new_endpoint_pool.status = kConnected;

                  new_endpoint_pool.pool.push_back(socket_sp);
                  socket_sp.reset();

                  if (max_conn_per_endpoint_ && new_endpoint_pool.pool.size() > max_conn_per_endpoint_)
                    new_endpoint_pool.pool.resize(max_conn_per_endpoint_);

                  if (new_endpoint_pool.pool.size() < min_conn_per_endpoint_)
                    // 需要补充连接至min_conn_per_endpoint_个
                    need_conn = min_conn_per_endpoint_ - new_endpoint_pool.pool.size();
                }

                // 补充连接
                if (need_conn)
                {
                  GlobalOutput.printf("%s[kConnected] need %u more connections\n",
                    endpoint_to_string(endpoint).c_str(), static_cast<uint32_t>(need_conn));

                  SocketSPVector pool;
                  for (size_t i=0; i<need_conn; i++)
                  {
                    socket_sp = socket_connect(ios_pool_.get_io_service(), endpoint, connect_timeout_);
                    if (!socket_sp)
                      break;
                    pool.push_back(socket_sp);
                    socket_sp.reset();
                  }

                  {
                    boost::recursive_mutex::scoped_lock guard(mutex_);
                    EndPointPool& new_endpoint_pool = map_[endpoint];
                    if (new_endpoint_pool.status != kConnected)
                      // 该池的状态被改变了
                      break;

                    new_endpoint_pool.pool.insert(new_endpoint_pool.pool.end(), pool.begin(), pool.end());
                    pool.clear();

                    if (max_conn_per_endpoint_ && new_endpoint_pool.pool.size() > max_conn_per_endpoint_)
                      new_endpoint_pool.pool.resize(max_conn_per_endpoint_);
                  }
                }
              }
            }
            break;

          case kDeleting:
          case kDeleted:
          default:
            assert(0);
            break;
          }
        }
      }
    }

    void probe()
    {
      __probe(false);
      set_probe();
    }

    void quick_probe()
    {
      __probe(true);
    }

  public:
    Impl(IOServicePool& ios_pool,
      size_t max_conn_per_endpoint,
      size_t min_conn_per_endpoint,
      size_t connect_timeout,
      size_t probe_cycle)
      :ios_pool_(ios_pool),
      max_conn_per_endpoint_(max_conn_per_endpoint),
      min_conn_per_endpoint_(min_conn_per_endpoint),
      connect_timeout_(connect_timeout),
      probe_cycle_(probe_cycle),
      mutex_(),
      map_(),
      stats_(),
      probe_thread_(),
      probe_io_service_(),
      probe_timer_(probe_io_service_),
      quick_probe_timer_(probe_io_service_)
    {
      assert(min_conn_per_endpoint_ < max_conn_per_endpoint_);
      start_probe();
    }

    ~Impl()
    {
      stop_probe();
      clear();
    }

    std::string get_status()const
    {
      std::ostringstream oss;

      boost::recursive_mutex::scoped_lock guard(mutex_);

      oss << stats_.to_string() << std::endl;

      EndPointPoolMap::const_iterator first = map_.begin();
      EndPointPoolMap::const_iterator last = map_.end();
      for (; first!=last; ++first)
      {
        const EndPoint& endpoint = (*first).first;
        const EndPointPool& endpoint_pool = (*first).second;

        oss << endpoint_to_string(endpoint) << ": status="
          << status_to_cstring(endpoint_pool.status)
          << ", pool size=" << endpoint_pool.pool.size() << std::endl;
      }

      return oss.str();
    }

    void add(const EndPoint& endpoint)
    {
      SocketSPVector tmp_pool;

      boost::recursive_mutex::scoped_lock guard(mutex_);
      EndPointPool& endpoint_pool = map_[endpoint];

      if (endpoint_pool.status == kDeleting
        || endpoint_pool.status == kDeleted)
      {
        endpoint_pool.status = kAdding;
        endpoint_pool.pool.swap(tmp_pool);
      }

      // 立即发起一次快速状态监测
      quick_probe();

      guard.unlock();
      clear_pool(&tmp_pool);
    }

    void add(const std::vector<EndPoint>& endpoints)
    {
      std::vector<SocketSPVector> tmp_pools;
      size_t size;

      boost::recursive_mutex::scoped_lock guard(mutex_);
      size = endpoints.size();
      tmp_pools.resize(size);
      for (size_t i=0; i<size; i++)
      {
        EndPointPool& endpoint_pool = map_[endpoints[i]];

        if (endpoint_pool.status == kDeleting
          || endpoint_pool.status == kDeleted)
        {
          endpoint_pool.status = kAdding;
          endpoint_pool.pool.swap(tmp_pools[i]);
        }
      }

      // 立即发起一次快速状态监测
      quick_probe();

      guard.unlock();
      for (size_t i=0; i<size; i++)
        clear_pool(&tmp_pools[i]);
    }

    void del(const EndPoint& endpoint)
    {
      SocketSPVector tmp_pool;

      boost::recursive_mutex::scoped_lock guard(mutex_);

      EndPointPoolMap::iterator it = map_.find(endpoint);
      if (it == map_.end())
        return;

      EndPointPool& endpoint_pool = (*it).second;

      if (endpoint_pool.status == kAdding
        || endpoint_pool.status == kConnected
        || endpoint_pool.status == kDisConnected)
      {
        endpoint_pool.status = kDeleting;
        endpoint_pool.pool.swap(tmp_pool);
      }

      guard.unlock();
      clear_pool(&tmp_pool);
    }

    bool get(const EndPoint& endpoint, SocketSP * socket_sp)
    {
      {
        SocketSPVector tmp_pool;

        boost::recursive_mutex::scoped_lock guard(mutex_);

        EndPointPool& endpoint_pool = map_[endpoint];
        if (endpoint_pool.status != kConnected)
        {
          // 保守,状态不为kConnected根本不会尝试去获取连接(即使这时服务可能已经恢复)
          stats_.got_conn_failure_++;
          //GlobalOutput.printf("%s: status is not [kConnected]\n",
          //  endpoint_to_string(endpoint).c_str());
          return false;
        }

        if (!endpoint_pool.pool.empty())
        {
          SocketSP& inner_socket_sp = endpoint_pool.pool.back();
          if (socket_is_closed(inner_socket_sp))
          {
            // 连接异常断开,关闭同一个池的所有连接
            endpoint_pool.pool.swap(tmp_pool);
            //GlobalOutput.printf("%s: no available socket in the pool\n",
            //  endpoint_to_string(endpoint).c_str());
          }
          else
          {
            socket_sp->swap(inner_socket_sp);
            endpoint_pool.pool.pop_back();

            stats_.got_from_pool_conn_++;
            //GlobalOutput.printf("%s: got a socket from the pool\n",
            //  endpoint_to_string(endpoint).c_str());
            return true;
          }
        }

        guard.unlock();
        clear_pool(&tmp_pool);
      }

      // 这里,连接池为空,或者连接池中的连接断开,立即尝试一次连接
      *socket_sp = socket_connect(ios_pool_.get_io_service(), endpoint, connect_timeout_);
      if (!(*socket_sp))
      {
        SocketSPVector tmp_pool;

        boost::recursive_mutex::scoped_lock guard(mutex_);
        // 连接失败,立即设置为kDisConnected
        EndPointPool& endpoint_pool = map_[endpoint];
        endpoint_pool.status = kDisConnected;
        endpoint_pool.pool.swap(tmp_pool);

        stats_.got_conn_failure_++;
        //GlobalOutput.printf("%s: got a new connection failed\n",
        //  endpoint_to_string(endpoint).c_str());

        guard.unlock();
        clear_pool(&tmp_pool);

        return false;
      }
      else
      {
        boost::recursive_mutex::scoped_lock guard(mutex_);
        stats_.got_from_created_conn_++;
        //GlobalOutput.printf("%s: got a new connection ok\n",
        //  endpoint_to_string(endpoint).c_str());
        return true;
      }
    }

    void put(SocketSP * socket_sp)
    {
      if (!(*socket_sp)->is_open())
        return;

      EndPoint endpoint = (*socket_sp)->remote_endpoint();

      boost::recursive_mutex::scoped_lock guard(mutex_);
      EndPointPool& endpoint_pool = map_[endpoint];

      if (endpoint_pool.status != kConnected)
      {
        (*socket_sp)->close();
        socket_sp->reset();
        return;
      }

      // 放入连接池
      endpoint_pool.pool.push_back(*socket_sp);
      socket_sp->reset();

      if (max_conn_per_endpoint_ && endpoint_pool.pool.size() > max_conn_per_endpoint_)
        endpoint_pool.pool.resize(max_conn_per_endpoint_);
    }

    void clear()
    {
      boost::recursive_mutex::scoped_lock guard(mutex_);
      map_.clear();
      stats_.clear();
    }
  };

  /************************************************************************/
  AsioPool::AsioPool(IOServicePool& ios_pool,
    size_t max_conn_per_endpoint,
    size_t min_conn_per_endpoint,
    size_t connect_timeout,
    size_t probe_cycle)
  {
    impl_ = new Impl(ios_pool,
      max_conn_per_endpoint, min_conn_per_endpoint, connect_timeout, probe_cycle);
  }

  AsioPool::~AsioPool()
  {
    delete impl_;
  }

  std::string AsioPool::get_status()const
  {
    return impl_->get_status();
  }

  void AsioPool::add(const EndPoint& endpoint)
  {
    impl_->add(endpoint);
  }

  void AsioPool::add(const std::vector<EndPoint>& endpoints)
  {
    impl_->add(endpoints);
  }

  void AsioPool::del(const EndPoint& endpoint)
  {
    impl_->del(endpoint);
  }

  bool AsioPool::get(const EndPoint& endpoint, SocketSP * socket_sp)
  {
    return impl_->get(endpoint, socket_sp);
  }

  void AsioPool::put(SocketSP * socket_sp)
  {
    impl_->put(socket_sp);
  }

  void AsioPool::clear()
  {
    impl_->clear();
  }

} } }
