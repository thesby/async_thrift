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
#include <AsyncCommon.h>

namespace apache { namespace thrift { namespace async {

  typedef boost::asio::ip::tcp::endpoint EndPoint;
  typedef boost::asio::ip::tcp::socket Socket;
  typedef boost::shared_ptr<Socket> SocketSP;

  struct EndPointConn
  {
    EndPoint endpoint;// AsioPool用来检索连接的主键
    SocketSP socket;
  };

  class AsioPool
  {
  private:
    class Impl;
    Impl * impl_;
  public:
    // ios_pool中的io_service用来创建socket,ios_pool必须处于running状态
    // max_conn_per_endpoint是每个EndPoint的最大连接数,0为不限制
    // probe_cycle是内部检测/保活的执行周期
    explicit AsioPool(IOServicePool& ios_pool,
      size_t max_conn_per_endpoint = 128,
      size_t probe_cycle = 10);
    ~AsioPool();

    // 获取连接池信息,可读
    std::string get_status()const;

    // 添加/删除一个服务地址
    void add(const EndPoint& endpoint);
    void del(const EndPoint& endpoint);

    // 获取/归还一个服务地址的连接
    bool get(EndPointConn& conn);
    void put(EndPointConn& conn);

    // 清空连接池所有连接
    void clear();
  };

} } } // namespace

#endif
