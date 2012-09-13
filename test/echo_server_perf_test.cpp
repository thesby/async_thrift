/** @file
 * @brief echo server performance test
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include "gen-cpp/AsyncEchoServer.h"
#include <server_benchmark.inl>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/date_time.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <transport/TSocket.h>
#include <transport/TBufferTransports.h>
#include <protocol/TBinaryProtocol.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::test;

static void pressure_test_thread(const std::string& host, int port)
{
  boost::posix_time::ptime begin, end;
  boost::posix_time::time_duration td;
  int ms;
  bool success;

  boost::shared_ptr<TSocket> socket(new TSocket(host, port));
  boost::shared_ptr<TTransport> transport(new TFramedTransport(socket));
  boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
  EchoServerClient client(protocol);
  Response _return;
  Request request;

  while (!g_stop_flag)
  {
    request.__isset.message = true;
    request.message = "test";

    begin = boost::posix_time::microsec_clock::local_time();
    try
    {
      if (!transport->isOpen())
        transport->open();

      client.echo(_return, request);
      success = true;
    }
    catch(apache::thrift::TException& e)
    {
      //printf("caught an apache::thrift::TException %s\n", e.what());
      success = false;
    }
    end = boost::posix_time::microsec_clock::local_time();
    td = end - begin;
    ms = static_cast<int>(td.total_milliseconds());

    if (success)
      ServerBenchmarkStat::instance()->inc_success();
    else
      ServerBenchmarkStat::instance()->inc_failure();
    ServerBenchmarkStat::instance()->inc_rtt(ms);
  }

  if (transport->isOpen())
    transport->close();
}

int main(int argc, char **argv)
{
  try
  {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "produce help message")
      ("host,h", po::value<std::string>()->default_value("localhost"), "host")
      ("port,p", po::value<int>()->default_value(12500), "port")
      ("threadpool-size,t", po::value<int>()->default_value(16), "thread pool size");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
      std::cout << desc << std::endl;
      return 0;
    }

    std::string host = vm["host"].as<std::string>();
    int port = vm["port"].as<int>();
    int threadpool_size = vm["threadpool-size"].as<int>();

    install_signal_handler();

    boost::thread_group thread_group;
    for (int i=0; i<threadpool_size; ++i)
    {
      thread_group.create_thread(boost::bind(pressure_test_thread, host, port));
    }
    thread_group.join_all();
  }
  catch (std::exception& e)
  {
    printf("caught: %s\n", e.what());
  }
  catch (...)
  {
    printf("caught: something\n");
  }
  return 0;
}
