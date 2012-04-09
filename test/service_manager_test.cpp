/** @file
* @brief service manager test
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <service_manager.h>
#include <gen-cpp/AsyncFacebookService.h>

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
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

  void run_ios_pool_thread(IOServicePool * ios_pool)
  {
    ios_pool->run();
  }

  void dump_thread(ServiceManager * sm)
  {
    while(!s_stop_flag)
    {
      s_stat.dump();

      std::string status = sm->get_status();
      printf("\n%s\n\n", status.c_str());

      boost::this_thread::sleep(boost::posix_time::seconds(2));
    }
  }

  void press_thread(ServiceManager * sm)
  {
    while (!s_stop_flag)
    {
      facebook::fb303::AsyncFacebookServiceClient client;
      SocketSP socket_sp;

      if (!sm->get(1, &socket_sp))
      {
        s_stat.inc_failure_get_conn();
        boost::this_thread::sleep(boost::posix_time::seconds(1));
      }
      else
      {
        client.attach(socket_sp);
        try
        {
          facebook::fb303::fb_status status = client.getStatus();
          if (status == facebook::fb303::ALIVE)
            s_stat.inc_success();
          else
            s_stat.inc_failure_rpc();

          client.detach();
          sm->put(1, &socket_sp);
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
      ("backends,b", po::value<std::string>()->default_value("sdl-adweb42:9102,sdl-adweb43:9102"),
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
    return 1;
  }

  /************************************************************************/
  IOServicePool ios_pool(16);
  s_ios_pool = &ios_pool;
  AsioPool asio_pool(ios_pool);
  ServiceManager sm(asio_pool);

  sm.add_backend(1, backends);

  /************************************************************************/
  signal(SIGINT, signal_handler);
  s_stop_flag = false;


  printf("testing service manager\n");
  boost::thread_group group;
  group.create_thread(boost::bind(run_ios_pool_thread, &ios_pool));
  group.create_thread(boost::bind(dump_thread, &sm));
  for (int i=0; i<thread_number; i++)
    group.create_thread(boost::bind(press_thread, &sm));
  group.join_all();
  printf("finished testing service manager\n");

  return 0;
}
