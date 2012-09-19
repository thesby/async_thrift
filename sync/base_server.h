/** @file
 * @brief base thrift server
 *
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef BASE_SERVER_H
#define BASE_SERVER_H

#include <server/TServer.h>
#include <Thrift.h>

//lint -esym(1712,BaseServer) default constructor not defined

namespace apache { namespace thrift { namespace sync {

  using ::apache::thrift::protocol::TProtocolFactory;
  using ::apache::thrift::server::TServer;
  using ::apache::thrift::TProcessor;
  using ::apache::thrift::transport::TServerTransport;
  using ::apache::thrift::transport::TTransportFactory;

  class BaseServer : public TServer
  {
    public:
      BaseServer(boost::shared_ptr<TProcessor> processor,
          boost::shared_ptr<TServerTransport> serverTransport,
          boost::shared_ptr<TTransportFactory> transportFactory,
          boost::shared_ptr<TProtocolFactory> protocolFactory);

      virtual ~BaseServer();

      virtual void serve() = 0;
      virtual void stop();

    protected:
      virtual bool startListen();
      virtual void stopListen();
      virtual void safeStopListen();
      virtual bool isListening()const;

      bool is_listen_;
      volatile bool stop_;

    public:
      static void yield();
  };

} } }

#endif
