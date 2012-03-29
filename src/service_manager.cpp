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
    // �����һ��������Ϣ
    struct ServiceEndPoint
    {
      std::string host;// ����,���ڴ�ӡ
      std::string port;// �˿�,���ڴ�ӡ
      std::string host_port;// ����:�˿�,���ڴ�ӡ
      EndPoint endpoint;// ��asioֱ��ʹ�õĵ�ַ

      bool local_host;// �Ƿ��Ǳ����ķ���
    };

    struct ServiceEndPointLess
    {
      bool operator()(const ServiceEndPoint& a, const ServiceEndPoint& b)const
      {
        // local_hostΪtrue������ǰ��,��ΰ��յ�ַ����
        return (a.local_host && !b.local_host)
          || (!(a.local_host && !b.local_host) && a.endpoint < b.endpoint);
      }
    };

    typedef std::set<ServiceEndPoint, ServiceEndPointLess> SEPSet;
    typedef std::vector<const ServiceEndPoint *> SEPPtrVector;

    struct Service
    {
      SEPSet sep_set;
      mutable SEPPtrVector sep_select_vec;// ����,�������ѡ��ʹ��
      mutable size_t sep_select_vec_count;// ����,�������ѡ��ʹ��

      Service() :sep_select_vec_count(0) {}
    };

    // ����id->���з���������Ϣ��ӳ��
    typedef std::map<int, Service> ServiceMap;

    mutable boost::recursive_mutex mutex_;// ����map_
    ServiceMap map_;

    AsioPool& asio_pool_;

    const std::string local_host_name_;// ����������

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

      for (size_t i=0; i<size; i++)
      {
        std::vector<std::string> host_port;
        boost::split(host_port, hosts_ports[i], boost::is_any_of(":"));

        if (host_port.size() != 2)
        {
          GlobalOutput.printf("invalid host and port: %s\n", hosts_ports[i].c_str());
          continue;
        }

        // ��������
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
        sep.local_host = (sep.host == local_host_name_);

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

      boost::recursive_mutex::scoped_lock guard(mutex_);
      map_[id].sep_set.swap(_set);
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
          old_set.insert(it, sep);
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
          old_set.erase(it);
      }
    }

    bool get(int id, SocketSP * socket_sp)
    {
      boost::recursive_mutex::scoped_lock guard(mutex_);
      Service& service = map_[id];
      const SEPSet& sep_set = service.sep_set;
      SEPPtrVector& sep_select_vec = service.sep_select_vec;
      size_t& sep_select_vec_count = service.sep_select_vec_count;

      // �����������������,���߸���������Ѿ�ʹ�ù�һ������,������ѡ���������
      if (sep_select_vec.size() != sep_set.size()
        || sep_select_vec_count > 1024)// magic number
      {
        sep_select_vec.clear();
        SEPSet::const_iterator first = sep_set.begin();
        SEPSet::const_iterator last = sep_set.end();
        for (; first!=last; ++first)
        {
          const ServiceEndPoint& sep = (*first);
          sep_select_vec.push_back(&sep);
        }
        sep_select_vec_count = 0;

        size_t i=0;
        size_t size = sep_select_vec.size();
        for (; i<size; i++)
        {
          if (!sep_select_vec[i]->local_host)
            break;
        }

        // �������,ǰ������local_host�ķ���,�󲿷��Ƿ�local_host�ķ���
        std::random_shuffle(sep_select_vec.begin(), sep_select_vec.begin()+i);
        std::random_shuffle(sep_select_vec.begin()+i, sep_select_vec.end());
      }

      // �����������ѡ��
      sep_select_vec_count++;

      size_t size = sep_select_vec.size();
      for (size_t i=0; i<size; i++)
      {
        if (asio_pool_.get(sep_select_vec[i]->endpoint, socket_sp))
          return true;
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

      oss << asio_pool_.get_status() << std::endl << std::endl;

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
          oss << sep.host_port << " -> " << sep.endpoint << std::endl;
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