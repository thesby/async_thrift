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

      // backends�ַ�����ʽ��:
      //   redis1:9100,redis2:9100,redis3:9100
      // ����/����/ɾ����˷����б���Ϣ
      void set_backend(int id, const std::string& backends);
      void add_backend(int id, const std::string& backends);
      void del_backend(int id, const std::string& backends);

      // ��ȡĳ������ID������
      // ����true,��ȡ�ɹ�
      // ����false,��ȡʧ��
      bool get(int id, SocketSP * socket_sp);
      // �Ż�����
      void put(int id, SocketSP * socket_sp);

      // ��ȡ���������Ϣ,�ɶ�
      std::string get_status()const;
  };

} } }

#endif
