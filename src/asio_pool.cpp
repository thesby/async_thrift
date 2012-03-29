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

    // ���Զ��socket�Ƿ��ѶϿ�
    // ע��:��ʹ��fb303.getStatus,����ʹ�ø��ײ��recvϵͳ���ü������,��ͨ�ø�Ч
    // ����:���еķ��������������׼���������ܼ����˿�,��Ϊ�˿ڴ�,��AsioPool��Ϊ�÷������
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
        // ����SO_SNDTIMEO,����������֮���connect����
        struct timeval timeout = {0, timeout_ms*1000};
        ::setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
      }

      // ����ʱ����������
      socket_sp->connect(endpoint, ec);

      if (ec)
      {
        socket_sp.reset();
        GlobalOutput.printf("connect %s failed: %s\n",
          endpoint_to_string(endpoint).c_str(), ec.message().c_str());
      }
      else
      {
        // ȡ��SO_SNDTIMEO
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
      kAdding = 0,// �����
      kConnected,// ���ӱ���,ֻ�д�״̬�ĳزſ���
      kDisConnected,// ���ӶϿ�
      kDeleting,// ��ɾ��
      kDeleted,// ��ɾ��
    };

    /**
    * ״̬ת���߼�:
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

      kStatus status;// ״̬
      SocketSPVector pool;// ��
    };

    typedef std::map<EndPoint, EndPointPool> EndPointPoolMap;

    IOServicePool& ios_pool_;
    const size_t max_conn_per_endpoint_;
    const size_t min_conn_per_endpoint_;
    const size_t connect_timeout_;
    const size_t probe_cycle_;

    mutable boost::recursive_mutex mutex_;// ����map_,stats_
    EndPointPoolMap map_;

    // ͳ����Ϣ
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

      uint64_t got_conn_failure_;// �������ʧ�ܵĴ���
      uint64_t got_from_pool_conn_;// �ӳ��л�����ӵĴ���
      uint64_t got_from_created_conn_;// ���½����ӻ�����ӵĴ���
      uint64_t connected_count_;// �ر�ΪkConnected�Ĵ���
      uint64_t disconnected_count_;// �ر�ΪkDisConnected�Ĵ���
    };
    Stats stats_;

    // ��̨ά������״̬���̼߳�����ر���
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
      EndPointPoolMap probe_map;// ��Ҫ���״̬�ĳ�
      std::vector<SocketSPVector> tmp_pools;
      size_t size;

      {
        // 1.����map_,����״̬
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
                // �����쳣�Ͽ�,�ر�ͬһ���ص���������
                tmp_pools.resize(tmp_pools.size() + 1);
                endpoint_pool.pool.swap(tmp_pools.back());

                GlobalOutput.printf("%s[kConnected] seems a disconnection\n",
                  endpoint_to_string(endpoint).c_str());
                // fall through
              }
              else
              {
                // ������
                GlobalOutput.printf("%s[kConnected] ok\n",
                  endpoint_to_string(endpoint).c_str());
                break;
              }
            }
            else
            {
              // ��Ϊ��
              GlobalOutput.printf("%s[kConnected]'s pool is empty\n",
                endpoint_to_string(endpoint).c_str());
              // fall through
            }

          case kAdding:
          case kDisConnected:
            // ����probe_map,��¼status,����statusֻ������kAdding,kConnected,kDisConnected
            // ����һ�׶δ���
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
            // ɾ����Ҳ������map_��,��Ϊ�ܿ��ܽ�����������
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

      // 2.��probe_map�е����ӽ��м��
      // ������ӳɹ�������kConnected״̬,��������kDisConnected״̬
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
            // �óص�״̬���ı���(ֻ�����ǵ�����del),���ֲ�����,������һ�ֵ�һ�׶δ���
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
                // ����ʧ��
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

                  // ����
                  guard.unlock();
                  // �ر�socket�������ܻ�����,��������������,��ͬ
                  clear_pool(&tmp_pool);
                }
              }
              else
              {
                // ���ӳɹ�
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
                    // �óص�״̬���ı���(ֻ�����ǵ�����del)
                    break;

                  new_endpoint_pool.status = kConnected;

                  new_endpoint_pool.pool.push_back(socket_sp);
                  socket_sp.reset();

                  if (max_conn_per_endpoint_ && new_endpoint_pool.pool.size() > max_conn_per_endpoint_)
                    new_endpoint_pool.pool.resize(max_conn_per_endpoint_);

                  if (new_endpoint_pool.pool.size() < min_conn_per_endpoint_)
                    // ��Ҫ����������min_conn_per_endpoint_��
                    need_conn = min_conn_per_endpoint_ - new_endpoint_pool.pool.size();
                }

                // ��������
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
                      // �óص�״̬���ı���
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

      // ��������һ�ο���״̬���
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

      // ��������һ�ο���״̬���
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
          // ����,״̬��ΪkConnected�������᳢��ȥ��ȡ����(��ʹ��ʱ��������Ѿ��ָ�)
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
            // �����쳣�Ͽ�,�ر�ͬһ���ص���������
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

      // ����,���ӳ�Ϊ��,�������ӳ��е����ӶϿ�,��������һ������
      *socket_sp = socket_connect(ios_pool_.get_io_service(), endpoint, connect_timeout_);
      if (!(*socket_sp))
      {
        SocketSPVector tmp_pool;

        boost::recursive_mutex::scoped_lock guard(mutex_);
        // ����ʧ��,��������ΪkDisConnected
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

      // �������ӳ�
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
