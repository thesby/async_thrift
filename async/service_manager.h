/** @file
 * @brief service manager
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef SERVICE_MANAGER_H
#define SERVICE_MANAGER_H

#include <asio_pool.h>

//lint -esym(1712,ServiceManager) default constructor not defined
//lint -esym(1732,ServiceManager) no assignment operator
//lint -esym(1733,ServiceManager) no copy constructor

namespace apache { namespace thrift { namespace async {

  class ServiceManager
  {
    private:
      class Impl;
      Impl * impl_;
    public:
      explicit ServiceManager(AsioPool& asio_pool);
      ~ServiceManager();

      // backends字符串格式如:
      //   redis1:9100,redis2:9100,redis3:9100
      // 设置/增加/删除后端服务列表信息
      void set_backend(int id, const std::string& backends);
      void add_backend(int id, const std::string& backends);
      void del_backend(int id, const std::string& backends);

      // 获取某个服务ID的连接
      // 返回true,获取成功
      // 返回false,获取失败
      bool get(int id, SocketSP * socket_sp);
      // 放回连接
      void put(int id, SocketSP * socket_sp);

      // 获取服务管理信息,可读
      std::string get_status()const;
  };

} } }

#endif
