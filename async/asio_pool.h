/** @file
 * @brief asio tcp socket pool
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef ASIO_POOL_H
#define ASIO_POOL_H

#include <io_service_pool.h>
#include <async_common.h>

namespace apache { namespace thrift { namespace async {

  typedef boost::asio::ip::tcp::endpoint EndPoint;
  typedef boost::asio::ip::tcp::socket Socket;
  typedef boost::shared_ptr<Socket> SocketSP;

  class AsioPool
  {
    private:
      class Impl;
      Impl * impl_;
    public:
      // ios_pool中的io_service用来创建socket,ios_pool必须处于running状态
      // max_conn_per_endpoint是每个EndPoint的最大连接数,0为不限制
      // min_conn_per_endpoint是每个EndPoint的最小连接数
      // connect_timeout是连接的超时时间
      // probe_cycle是内部检测/保活的执行周期
      explicit AsioPool(IOServicePool& ios_pool,
          size_t max_conn_per_endpoint = 128,
          size_t min_conn_per_endpoint = 20,
          size_t connect_timeout = 20,
          size_t probe_cycle = 10);
      ~AsioPool();

      // 获取连接池信息,可读
      std::string get_status()const;

      // 添加/删除一个服务地址
      void add(const EndPoint& endpoint);
      void add(const std::vector<EndPoint>& endpoints);
      void del(const EndPoint& endpoint);

      // 获取/归还一个服务地址的连接
      bool get(const EndPoint& endpoint, SocketSP * socket_sp);
      void put(SocketSP * socket_sp);

      // 清空连接池所有连接
      void clear();
  };

} } } // namespace

#endif
