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

namespace {

  bool s_stop_flag = false;
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
      unsigned long success_bak;
      unsigned long failure_get_conn_bak;
      unsigned long failure_rpc_bak;
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
    unsigned long success;
    unsigned long failure_get_conn;
    unsigned long failure_rpc;
  } s_stat;

  std::vector<EndPoint> s_endpoints;

  void run_ios_pool_thread(IOServicePool * ios_pool)
  {
    ios_pool->run();
  }

  void dump_thread(const AsioPool * asio_pool)
  {
    while(!s_stop_flag)
    {
      s_stat.dump();

      std::string status = asio_pool->get_status();
      printf("\n%s\n\n", status.c_str());

      boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
  }

  void press_thread(AsioPool * asio_pool)
  {
    while (!s_stop_flag)
    {
      ::facebook::fb303::AsyncFacebookServiceClient client;
      EndPoint endpoint;
      SocketSP socket_sp;
      endpoint = s_endpoints[static_cast<size_t>(rand()) % s_endpoints.size()];

      if (!asio_pool->get(endpoint, &socket_sp))
      {
        s_stat.inc_failure_get_conn();
        boost::this_thread::sleep(boost::posix_time::seconds(1));
      }
      else
      {
        client.attach(socket_sp);
        try
        {
          ::facebook::fb303::fb_status status = client.getStatus();
          if (status == ::facebook::fb303::ALIVE)
            s_stat.inc_success();
          else
            s_stat.inc_failure_rpc();

          client.detach();
          asio_pool->put(&socket_sp);
          boost::this_thread::sleep(boost::posix_time::microseconds(rand() % 100));
        }
        catch (std::exception& e)
        {
          socket_sp->close();
          socket_sp.reset();
          s_stat.inc_failure_rpc();

          boost::this_thread::sleep(boost::posix_time::seconds(1));
          printf("getStatus error: %s\n", e.what());
        }
      }
    }
  }
}

int main(int argc, char ** argv)
{
  std::string backends;
  int thread_number;

  try
  {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    (void)desc.add_options()
      ("help,h", "produce help message")
      ("backends,b", po::value<std::string>()->default_value("sdl-adweb42:9094,sdl-adweb43:9094"),
       "test backends")
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
    return 0;
  }

  /************************************************************************/
  std::vector<std::string> backend_list;
  boost::asio::io_service ios;
  boost::asio::ip::tcp::resolver resolver(ios);

  (void)boost::split(backend_list, backends, boost::is_any_of(","));
  for (size_t i=0; i<backend_list.size(); i++)
  {
    std::vector<std::string> host_port;
    (void)boost::split(host_port, backend_list[i], boost::is_any_of(":"));
    if (host_port.size() != 2)
    {
      printf("backend error: %s\n", backend_list[i].c_str());
      continue;
    }

    boost::system::error_code ec;
    const std::string& host = host_port[0];
    const std::string& port = host_port[1];
    boost::asio::ip::tcp::resolver::query query(host, port);
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
  IOServicePool ios_pool(static_cast<size_t>(thread_number));
  //lint --e(789) Assigning address of auto variable 'ios_pool' to static 
  s_ios_pool = &ios_pool;
  AsioPool asio_pool(ios_pool);
  asio_pool.add(s_endpoints);

  /************************************************************************/
  (void)signal(SIGINT, signal_handler);

  printf("testing AsioPool\n");
  boost::thread_group group;
  (void)group.create_thread(boost::bind(run_ios_pool_thread, &ios_pool));
  (void)group.create_thread(boost::bind(dump_thread, &asio_pool));
  for (int i=0; i<thread_number; i++)
    (void)group.create_thread(boost::bind(press_thread, &asio_pool));
  group.join_all();
  printf("finished testing AsioPool\n");

  return 0;
}
