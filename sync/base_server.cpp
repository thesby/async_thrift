/** @file
 * @brief base thrift server
 *
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include "base_server.h"
#include <unistd.h>

namespace apache { namespace thrift { namespace sync {

  using ::apache::thrift::GlobalOutput;
  using ::apache::thrift::transport::TTransportException;

  BaseServer::BaseServer(
      boost::shared_ptr<TProcessor> processor,
      boost::shared_ptr<TServerTransport> serverTransport,
      boost::shared_ptr<TTransportFactory> transportFactory,
      boost::shared_ptr<TProtocolFactory> protocolFactory)
    : TServer(processor, serverTransport, transportFactory, protocolFactory),
    is_listen_(false), stop_(false)
  {}

  BaseServer::~BaseServer()
  {}

  void BaseServer::stop()
  {
    GlobalOutput("BaseServer::stop()");
    stop_ = true;
    //lint --e(26) lint bug
    //lint --e(522) lint bug
    serverTransport_->interrupt();
  }

  bool BaseServer::startListen()
  {
    if (is_listen_)
      return true;

    try
    {
      // Start the server listening
      serverTransport_->listen();
      is_listen_ = true;
      GlobalOutput("BaseServer::startListen()");
      return true;
    }
    catch (TTransportException& e)
    {
      GlobalOutput.printf("BaseServer::startListen() TTransportException: %s", e.what());
      return false;
    }
  }

  void BaseServer::stopListen()
  {
    if (!is_listen_)
      return;

    //lint --e(118) lint bug
    //lint --e(1013) lint bug
    serverTransport_->close();
    is_listen_ = false;
    GlobalOutput("BaseServer::stopListen() close server transport");
  }

  void BaseServer::safeStopListen()
  {
    if (!is_listen_)
      return;

    try
    {
      //lint --e(118) lint bug
      //lint --e(1013) lint bug
      serverTransport_->close();
      is_listen_ = false;
      GlobalOutput("BaseServer::safeStopListen() close server transport");
    }
    catch (...) {}
  }

  bool BaseServer::isListening()const
  {
    return is_listen_;
  }

  void BaseServer::yield()
  {
    (void)usleep(100000);
  }

} } }
