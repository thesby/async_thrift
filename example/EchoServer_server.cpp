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
#include "AsyncThriftServer.h"
#include "EchoServer.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::concurrency;
using namespace ::apache::thrift::async;
using namespace com::langtaojin::adgaga;

class EchoServerHandler : virtual public EchoServerNull {
public:
  EchoServerHandler() {
    // Your initialization goes here
  }

  void echo(Response& _return, const Request& request) {
    // Your implementation goes here
    _return.__isset.message = true;
    _return.message = request.message;
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
      ("help", "produce help message")
      ("port,p", po::value<int>()->default_value(12500), "listening port")
      ("server-model,s", po::value<std::string>()->default_value("asio"), "server model: asio, threaded, threadpool")
      ("threadpool-size,t", po::value<int>()->default_value(128), "thread pool size(only for asio/threadpool model)");

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

    boost::shared_ptr<EchoServerHandler> handler(new EchoServerHandler());
    boost::shared_ptr<TProcessor> processor(new EchoServerProcessor(handler));

    signal(SIGINT, signal_handler);

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
    else
    {
      std::cout << "AsyncThriftServer" << std::endl;

      boost::asio::io_service io_service;
      boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor(new boost::asio::ip::tcp::acceptor(io_service));

      boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), port);
      acceptor->open(endpoint.protocol());
      boost::asio::socket_base::reuse_address option(true);
      acceptor->set_option(option);
      acceptor->bind(endpoint);
      acceptor->listen();

      s_server.reset(new AsyncThriftServer(processor, acceptor, threadpool_size, 0));
      s_server->serve();
      s_server.reset();
    }
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

