/** @file
* @brief asio tcp socket pool test
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <asio_pool.h>
#include <gen-cpp/AsyncFacebookService.h>

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

using namespace ::apache::thrift::async;

namespace
{
  bool s_stop_flag = true;
  IOServicePool * s_ios_pool = NULL;

  void signal_handler(int)
  {
    s_stop_flag = true;
    if (s_ios_pool)
    {
      s_ios_pool->stop();
      s_ios_pool = NULL;
    }
  }

  struct Stat
  {
    Stat()
    {
      clear();
    }

    void clear()
    {
      boost::mutex::scoped_lock guard(mutex);
      success = 0;
      failure_get_conn = 0;
      failure_rpc = 0;
    }

    void inc_success()
    {
      boost::mutex::scoped_lock guard(mutex);
      success++;
    }

    void inc_failure_get_conn()
    {
      boost::mutex::scoped_lock guard(mutex);
      failure_get_conn++;
    }

    void inc_failure_rpc()
    {
      boost::mutex::scoped_lock guard(mutex);
      failure_rpc++;
    }

    void dump()const
    {
      int64_t success_bak;
      int64_t failure_get_conn_bak;
      int64_t failure_rpc_bak;
      {
        boost::mutex::scoped_lock guard(mutex);
        success_bak = success;
        failure_get_conn_bak = failure_get_conn;
        failure_rpc_bak = failure_rpc;
      }
      printf("success=%lu\n", success_bak);
      printf("failure_get_conn=%lu\n", failure_get_conn_bak);
      printf("failure_rpc=%lu\n", failure_rpc_bak);
    }

    mutable boost::mutex mutex;
    int64_t success;
    int64_t failure_get_conn;
    int64_t failure_rpc;
  } s_stat;

  std::vector<EndPoint> s_endpoints;

  void run_ios_pool_thread(IOServicePool * ios_pool)
  {
    ios_pool->run();
  }

  void dump_thread(AsioPool * asio_pool)
  {
    while(!s_stop_flag)
    {
      s_stat.dump();

      std::string status = asio_pool->get_status();
      printf("%s\n", status.c_str());

      boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
  }

  void press_thread(AsioPool * asio_pool)
  {
    while (!s_stop_flag)
    {
      facebook::fb303::AsyncFacebookServiceClient client;
      EndPointConn conn;
      conn.endpoint = s_endpoints[rand() % s_endpoints.size()];

      if (!asio_pool->get(conn))
      {
        s_stat.inc_failure_get_conn();
      }
      else
      {
        client.attach(conn.socket);
        try
        {
          facebook::fb303::fb_status status = client.getStatus();
          if (status == facebook::fb303::ALIVE)
            s_stat.inc_success();
          else
            s_stat.inc_failure_rpc();

          client.detach();
          asio_pool->put(conn);
        }
        catch (std::exception& e)
        {
          conn.socket->close();
          conn.socket.reset();
          s_stat.inc_failure_rpc();

          printf("getStatus error: %s\n", e.what());
        }
      }

      boost::this_thread::sleep(boost::posix_time::microseconds(rand() % 100));
    }
  }
}

int main(int argc, char **argv)
{
  std::string backends;
  int thread_number;

  try
  {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help,h", "produce help message")
      ("backends,b", po::value<std::string>()->default_value("sdl-redis17:9094,sdl-redis18:9094"), "test backends")
      ("thread_number,t", po::value<int>()->default_value(1), "test thread number");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
      std::cout << desc << std::endl;
      return 0;
    }

    backends = vm["backends"].as<std::string>();
    thread_number = vm["thread_number"].as<int>();
  }
  catch (std::exception& e)
  {
    std::cout << e.what() << std::endl;
    return 1;
  }

  /************************************************************************/
  std::vector<std::string> backend_list;
  boost::asio::io_service ios;
  boost::asio::ip::tcp::resolver resolver(ios);

  boost::split(backend_list, backends, boost::is_any_of(","));
  for (size_t i=0; i<backend_list.size(); i++)
  {
    std::vector<std::string> host_port;
    boost::split(host_port, backend_list[i], boost::is_any_of(":"));
    if (host_port.size() != 2)
    {
      printf("backend error: %s\n", backend_list[i].c_str());
      continue;
    }

    boost::system::error_code ec;
    boost::asio::ip::tcp::resolver::query query(host_port[0], host_port[1]);
    boost::asio::ip::tcp::resolver::iterator it = resolver.resolve(query, ec);
    if (ec)
    {
      printf("resolve %s error: %s\n", backend_list[i].c_str(), ec.message().c_str());
      continue;
    }

    s_endpoints.push_back(*it);
    printf("resolved %s\n", backend_list[i].c_str());
  }

  if (s_endpoints.size() == 0)
    return 0;

  /************************************************************************/
  IOServicePool ios_pool(16);
  s_ios_pool = &ios_pool;
  AsioPool asio_pool(ios_pool);

  for (size_t i=0; i<s_endpoints.size(); i++)
  {
    asio_pool.add(s_endpoints[i]);
  }

  /************************************************************************/
  signal(SIGINT, signal_handler);
  s_stop_flag = false;


  printf("testing asio pool\n");
  boost::thread_group group;
  group.create_thread(boost::bind(run_ios_pool_thread, &ios_pool));
  group.create_thread(boost::bind(dump_thread, &asio_pool));
  for (int i=0; i<thread_number; i++)
    group.create_thread(boost::bind(press_thread, &asio_pool));
  group.join_all();
  printf("finished testing asio pool\n");

  return 0;
}