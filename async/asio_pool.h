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
      // ios_pool�е�io_service��������socket,ios_pool���봦��running״̬
      // max_conn_per_endpoint��ÿ��EndPoint�����������,0Ϊ������
      // min_conn_per_endpoint��ÿ��EndPoint����С������
      // connect_timeout�����ӵĳ�ʱʱ��
      // probe_cycle���ڲ����/�����ִ������
      explicit AsioPool(IOServicePool& ios_pool,
          size_t max_conn_per_endpoint = 128,
          size_t min_conn_per_endpoint = 20,
          size_t connect_timeout = 20,
          size_t probe_cycle = 10);
      ~AsioPool();

      // ��ȡ���ӳ���Ϣ,�ɶ�
      std::string get_status()const;

      // ���/ɾ��һ�������ַ
      void add(const EndPoint& endpoint);
      void add(const std::vector<EndPoint>& endpoints);
      void del(const EndPoint& endpoint);

      // ��ȡ/�黹һ�������ַ������
      bool get(const EndPoint& endpoint, SocketSP * socket_sp);
      void put(SocketSP * socket_sp);

      // ������ӳ���������
      void clear();
  };

} } } // namespace

#endif
