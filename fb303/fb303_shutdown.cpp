/** @file
 * @brief a command line tool that invoke "shutdown" method
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include <gen-cpp/FacebookService.h>
#include <thrift_pool.h>
#include <stdio.h>
#include <iostream>
#include <boost/program_options.hpp>

using namespace ::facebook::fb303;
using namespace ::apache::thrift::sync;

int main(int argc, char ** argv)
{
  try
  {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    (void)desc.add_options()
      ("help", "produce help message")
      ("host,h", po::value<std::string>()->default_value("localhost"), "host")
      ("port,p", po::value<int>()->default_value(12500), "listening port")
      ("framed,f", "whether TFramedTransport or TBufferedTransport");

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
    bool framed = (vm.count("framed") != 0);

    ThriftConnection<FacebookServiceClient> conn(host, port, framed, true, 100, 100, 100);
    FacebookServiceClient * client = conn.client();
    if (client)
    {
      client->shutdown();
      printf("shutdown %s:%d ok\n", host.c_str(), port);
    }
    else
    {
      printf("not connected\n");
    }
  }
  catch (std::exception& e)
  {
    printf("caught: %s\n", e.what());
  }

  return 0;
}
