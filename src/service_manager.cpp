/** @file
* @brief service manager
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <service_manager.h>
#include <set>
#include <map>
#include <algorithm>
#include <boost/algorithm/string.hpp>

namespace apache { namespace thrift { namespace async {

  class ServiceManager::Impl
  {
  private:
    // 服务的一个主机信息
    struct ServiceEndPoint
    {
      std::string host;// 主机,用于打印
      std::string port;// 端口,用于打印
      std::string host_port;// 主机:端口,用于打印
      EndPoint endpoint;// 供asio直接使用的地址

      bool local_host;// 是否是本机的服务

      mutable size_t success, failure;// 成功/失败计数器
      ServiceEndPoint() :success(0), failure(0) {}
    };

    struct ServiceEndPointLess
    {
      bool operator()(const ServiceEndPoint& a, const ServiceEndPoint& b)const
      {
        // local_host为true的排在前面,其次按照地址排序
        if (a.local_host && !b.local_host)
          return true;
        if (!a.local_host && b.local_host)
          return false;
        assert(a.local_host == b.local_host);
        return a.endpoint < b.endpoint;
      }
    };

    typedef std::set<ServiceEndPoint, ServiceEndPointLess> SEPSet;
    typedef std::vector<const ServiceEndPoint *> SEPPtrVector;

    struct Service
    {
      SEPSet sep_set;
      mutable size_t local_host_index;
      mutable SEPPtrVector sep_select_vec;// 辅助,用于随机选择使用
      mutable size_t sep_select_vec_count;// 辅助,用于随机选择使用

      Service() :local_host_index(0), sep_select_vec_count(0) {}
    };

    // 服务id->所有服务主机信息的映射
    typedef std::map<int, Service> ServiceMap;

    mutable boost::recursive_mutex mutex_;// 保护map_
    ServiceMap map_;

    AsioPool& asio_pool_;

    const std::string local_host_name_;// 本机主机名

  private:
    void parse_backends(const std::string& backends, SEPSet * _set)
    {
      _set->clear();

      std::vector<std::string> hosts_ports;
      boost::split(hosts_ports, backends, boost::is_any_of(","));

      size_t size = hosts_ports.size();
      boost::asio::io_service ios;
      boost::asio::ip::tcp::resolver resolver(ios);
      boost::system::error_code ec;

      for (size_t i = 0; i < size; i++)
      {
        std::vector<std::string> host_port;
        boost::split(host_port, hosts_ports[i], boost::is_any_of(":"));

        if (host_port.size() != 2)
        {
          GlobalOutput.printf("invalid host and port: %s\n", hosts_ports[i].c_str());
          continue;
        }

        // 域名解析
        boost::asio::ip::tcp::resolver::query query(host_port[0], host_port[1]);
        boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query, ec);

        if (ec)
        {
          GlobalOutput.printf("resolve host %s error: %s\n",
            hosts_ports[i].c_str(), ec.message().c_str());
          continue;
        }

        ServiceEndPoint sep;
        sep.host = host_port[0];
        sep.port = host_port[1];
        sep.host_port = hosts_ports[i];
        sep.endpoint = iter->endpoint();
        sep.local_host = (sep.host == local_host_name_) || (sep.host == "localhost");

        _set->insert(sep);
      }
    }

  public:
    explicit Impl(AsioPool& asio_pool)
      :asio_pool_(asio_pool),
      local_host_name_(boost::asio::ip::host_name())
    {
    }

    ~Impl()
    {
    }

    void set_backend(int id, const std::string& backends)
    {
      SEPSet _set;
      parse_backends(backends, &_set);

      SEPSet::const_iterator first, last;

      boost::recursive_mutex::scoped_lock guard(mutex_);
      SEPSet& old_set = map_[id].sep_set;

      // 添加old_set中没有, _set中有的
      first = _set.begin();
      last = _set.end();
      for (; first!=last; ++first)
      {
        const ServiceEndPoint& sep = (*first);
        SEPSet::iterator it = old_set.find(sep);
        if (it == old_set.end())
          asio_pool_.add(sep.endpoint);
      }

      // 删除old_set中有, _set中没有的
      first = old_set.begin();
      last = old_set.end();
      for (; first!=last; ++first)
      {
        const ServiceEndPoint& sep = (*first);
        SEPSet::iterator it = _set.find(sep);
        if (it == _set.end())
          asio_pool_.del(sep.endpoint);
      }

      old_set.swap(_set);
    }

    void add_backend(int id, const std::string& backends)
    {
      SEPSet _set;
      parse_backends(backends, &_set);

      SEPSet::const_iterator first = _set.begin();
      SEPSet::const_iterator last = _set.end();

      boost::recursive_mutex::scoped_lock guard(mutex_);
      SEPSet& old_set = map_[id].sep_set;

      for (; first!=last; ++first)
      {
        const ServiceEndPoint& sep = (*first);
        SEPSet::iterator it = old_set.find(sep);
        if (it == old_set.end())
        {
          old_set.insert(it, sep);
          asio_pool_.add(sep.endpoint);
        }
      }
    }

    void del_backend(int id, const std::string& backends)
    {
      SEPSet _set;
      parse_backends(backends, &_set);

      SEPSet::const_iterator first = _set.begin();
      SEPSet::const_iterator last = _set.end();

      boost::recursive_mutex::scoped_lock guard(mutex_);
      SEPSet& old_set = map_[id].sep_set;

      for (; first!=last; ++first)
      {
        const ServiceEndPoint& sep = (*first);
        SEPSet::iterator it = old_set.find(sep);
        if (it != old_set.end())
        {
          old_set.erase(it);
          asio_pool_.del(sep.endpoint);
        }
      }
    }

    bool get(int id, SocketSP * socket_sp)
    {
      boost::recursive_mutex::scoped_lock guard(mutex_);
      Service& service = map_[id];
      const SEPSet& sep_set = service.sep_set;
      size_t& local_host_index = service.local_host_index;
      SEPPtrVector& sep_select_vec = service.sep_select_vec;
      size_t& sep_select_vec_count = service.sep_select_vec_count;

      // 如果服务数量有增减,或者该随机方案已经使用过一定次数,则重新选定随机方案
      if (sep_select_vec.size() != sep_set.size()
        || sep_select_vec_count > 4096)// magic number
      {
        GlobalOutput.printf("service manager status:\n%s\n",
          get_status().c_str());

        sep_select_vec.clear();
        SEPSet::const_iterator first = sep_set.begin();
        SEPSet::const_iterator last = sep_set.end();
        for (; first!=last; ++first)
        {
          const ServiceEndPoint& sep = (*first);
          sep_select_vec.push_back(&sep);
        }
        sep_select_vec_count = 0;

        size_t size = sep_select_vec.size();
        for (local_host_index = 0; local_host_index < size; local_host_index++)
        {
          if (!sep_select_vec[local_host_index]->local_host)
            break;
        }

        // 随机打乱,前部分是local_host的服务,后部分是非local_host的服务
        std::random_shuffle(sep_select_vec.begin(), sep_select_vec.begin()+local_host_index);
        std::random_shuffle(sep_select_vec.begin()+local_host_index, sep_select_vec.end());
      }

      const ServiceEndPoint * sep_ptr;
      // 按照随机方案选择(本机)
      for (size_t i=0; i<local_host_index; i++)
      {
        sep_ptr = sep_select_vec[sep_select_vec_count++ % local_host_index];
        if (asio_pool_.get(sep_ptr->endpoint, socket_sp))
        {
          sep_ptr->success++;
          return true;
        }
        else
        {
          sep_ptr->failure++;
        }
      }

      // 按照随机方案选择(非本机)
      size_t size = sep_select_vec.size();
      size_t non_local_size = size - local_host_index;
      for (size_t i = local_host_index; i < size; i++)
      {
        sep_ptr = sep_select_vec[local_host_index + (sep_select_vec_count++ % non_local_size)];
        if (asio_pool_.get(sep_ptr->endpoint, socket_sp))
        {
          sep_ptr->success++;
          return true;
        }
        else
        {
          sep_ptr->failure++;
        }
      }

      return false;
    }

    void put(int id, SocketSP * socket_sp)
    {
      asio_pool_.put(socket_sp);
    }

    std::string get_status()const
    {
      std::ostringstream oss;

      boost::recursive_mutex::scoped_lock guard(mutex_);

      oss << asio_pool_.get_status() << std::endl;

      ServiceMap::const_iterator first1 = map_.begin();
      ServiceMap::const_iterator last1 = map_.end();
      for (; first1!=last1; ++first1)
      {
        int id = (*first1).first;
        const SEPSet& sep_set = (*first1).second.sep_set;

        SEPSet::const_iterator first2 = sep_set.begin();
        SEPSet::const_iterator last2 = sep_set.end();

        oss << "id=" << id << ":" << std::endl;

        for (; first2!=last2; ++first2)
        {
          const ServiceEndPoint& sep = (*first2);
          oss << sep.host_port << " -> " << sep.endpoint << ' '
            << (sep.local_host?"local":"non-local") << ' '
            << "success=" << sep.success << ' '
            << "failure=" << sep.failure << ' '
            << std::endl;
        }
      }

      return oss.str();
    }
  };

  /************************************************************************/
  ServiceManager::ServiceManager(AsioPool& asio_pool)
  {
    impl_ = new Impl(asio_pool);
  }

  ServiceManager::~ServiceManager()
  {
    delete impl_;
  }

  void ServiceManager::set_backend(int id, const std::string& backends)
  {
    impl_->set_backend(id, backends);
  }

  void ServiceManager::add_backend(int id, const std::string& backends)
  {
    impl_->add_backend(id, backends);
  }

  void ServiceManager::del_backend(int id, const std::string& backends)
  {
    impl_->del_backend(id, backends);
  }

  bool ServiceManager::get(int id, SocketSP * socket_sp)
  {
    return impl_->get(id, socket_sp);
  }

  void ServiceManager::put(int id, SocketSP *socket_sp)
  {
    impl_->put(id, socket_sp);
  }

  std::string ServiceManager::get_status()const
  {
    return impl_->get_status();
  }

} } }
