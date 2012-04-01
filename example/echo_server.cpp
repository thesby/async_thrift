/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <signal.h>
#include <iostream>
#include <boost/program_options.hpp>
#include <protocol/TBinaryProtocol.h>
#include <server/TThreadedServer.h>
#include <server/TThreadPoolServer.h>
#include <concurrency/PosixThreadFactory.h>
#include <transport/TServerSocket.h>
#include <transport/TBufferTransports.h>
#include <async_server.h>
#include "gen-cpp/AsyncEchoServer.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::concurrency;
using namespace ::apache::thrift::async;
using namespace com::langtaojin::adgaga;


class EchoServerHandler : public EchoServerNull
{
public:
  void echo(Response& _return, const Request& request)
  {
    sleep(1);
    _return.__isset.message = true;
    _return.message = request.message;
  }
  int32_t echo2(const int32_t i) {
    return i;
  }
  void echo3(std::string& _return, const std::string& str) {
    _return = str;
  }
  void echo4(std::string& /* _return */, const int32_t /* i1 */, const int64_t /* i2 */) {
    return;
  }
  void void_func() {
    return;
  }
  void void_func2(const Request& /* request */, const std::string& /* str */) {
    return;
  }
  void oneway_func() {
    return;
  }
  void oneway_func2(const Request& /* request */, const std::string& /* str */) {
    return;
  }
};


class AsyncEchoServerHandler : public AsyncEchoServerNull
{
public:
  virtual void async_echo(Response& _return, const Request& request, ::apache::thrift::async::AsyncRPCCallback callback)
  {
    _return.__isset.message = true;
    _return.message = request.message;
    callback(boost::system::error_code());
  }
};


static boost::shared_ptr<TServer> s_server;

static void signal_handler(int)
{
  if (s_server)
    s_server->stop();
}

int main(int argc, char **argv)
{
  try
  {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help,h", "produce help message")
      ("port,p", po::value<int>()->default_value(12500), "listening port")
      ("server-model,s", po::value<std::string>()->default_value("async"), "server model: async, sync, threaded, threadpool")
      ("threadpool-size,t", po::value<int>()->default_value(32), "thread pool size(only for async/asio/threadpool model)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
      std::cout << desc << std::endl;
      return 0;
    }

    int port = vm["port"].as<int>();
    std::string server_model = vm["server-model"].as<std::string>();
    int threadpool_size = vm["threadpool-size"].as<int>();

    signal(SIGINT, signal_handler);

    boost::shared_ptr<EchoServerHandler> handler(new EchoServerHandler());
    boost::shared_ptr<TProcessor> processor(new EchoServerProcessor(handler));
    boost::shared_ptr<AsyncEchoServerHandler> async_handler(new AsyncEchoServerHandler());
    boost::shared_ptr<AsyncProcessor> async_processor(new AsyncEchoServerProcessor(async_handler));

    if (server_model == "threaded")
    {
      std::cout << "TThreadedServer" << std::endl;
      boost::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
      boost::shared_ptr<TTransportFactory> transportFactory(new TFramedTransportFactory());
      boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

      s_server.reset(new TThreadedServer(processor,
        serverTransport,
        transportFactory,
        protocolFactory));
      s_server->serve();
      s_server.reset();
    }
    else if (server_model == "threadpool")
    {
      std::cout << "TThreadPoolServer" << std::endl;

      boost::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
      boost::shared_ptr<TTransportFactory> transportFactory(new TFramedTransportFactory());
      boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
      boost::shared_ptr<ThreadManager> thread_manager
        = ThreadManager::newSimpleThreadManager(threadpool_size, threadpool_size);
      boost::shared_ptr<PosixThreadFactory> thread_factory(new PosixThreadFactory());
      thread_manager->threadFactory(thread_factory);

      s_server.reset(new TThreadPoolServer(processor,
        serverTransport,
        transportFactory,
        protocolFactory,
        thread_manager));

      thread_manager->start();
      s_server->serve();
      s_server.reset();
    }
    else if (server_model == "async")
    {
      std::cout << "AsyncThriftServer async RPC" << std::endl;

      boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v6(), port);
      IOServicePool pool(threadpool_size);
      s_server.reset(new AsyncThriftServer(async_processor, endpoint, pool));
      s_server->serve();
      s_server.reset();
    }
    else
    {
      std::cout << "AsyncThriftServer sync RPC" << std::endl;

      boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v6(), port);
      IOServicePool pool(threadpool_size);
      s_server.reset(new AsyncThriftServer(processor, endpoint, pool));
      s_server->serve();
      s_server.reset();
    }
    std::cout << "stopped" << std::endl;
  }
  catch (std::exception& e)
  {
    std::cout << "caught: " << e.what() << std::endl;
  }
  catch (...)
  {
    std::cout << "caught: something" << std::endl;
  }

  return 0;
}
