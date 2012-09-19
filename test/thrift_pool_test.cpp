/** @file
 * @brief thrift pool and thrift pool for thread specific storage test
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include "thrift_pool.h"
#include "thrift_pool_tss.h"
#include <server_benchmark.inl>
#include <signal.h>
#include <stdio.h>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

using namespace ::facebook::fb303;
using namespace ::apache::thrift::sync;

typedef ThriftPool<FacebookServiceClient> ThriftPoolType;
typedef ThriftPoolTss<FacebookServiceClient> ThriftPoolTssType;

static void pressure_test_thread(ThriftPoolType * pool)
{
  while (!g_stop_flag)
  {
    ThriftPoolType::ThriftConnectionType * conn = pool->get();

    if (conn)
    {
      try
      {
        FacebookServiceClient * client = conn->client();
        if (client)
        {
          (void)client->getStatus();
          ServerBenchmarkStat::instance()->inc_success();
          boost::this_thread::sleep(boost::posix_time::microseconds(rand() % 100));
        }
        else
        {
          ServerBenchmarkStat::instance()->inc_failure();
        }
      }
      catch (::apache::thrift::TException& /*e*/)
      {
        conn->close();
        ServerBenchmarkStat::instance()->inc_failure();
      }

      (void)pool->put(conn);
    }
    else
    {
      ServerBenchmarkStat::instance()->inc_failure();
    }
  }
}

static void pressure_test_thread_tss(ThriftPoolTssType * pool)
{
  while (!g_stop_flag)
  {
    ThriftPoolTssType::ThriftConnectionType * conn = pool->get_tss();

    if (conn)
    {
      try
      {
        FacebookServiceClient * client = conn->client();
        if (client)
        {
          (void)client->getStatus();
          ServerBenchmarkStat::instance()->inc_success();
          boost::this_thread::sleep(boost::posix_time::microseconds(rand() % 100));
        }
        else
        {
          ServerBenchmarkStat::instance()->inc_failure();
        }
      }
      catch (::apache::thrift::TException& /*e*/)
      {
        conn->close();
        ServerBenchmarkStat::instance()->inc_failure();
      }

      // need not to put back, because 'conn' is bound to each thread
    }
    else
    {
      ServerBenchmarkStat::instance()->inc_failure();
    }
  }
}

int main(int argc, char ** argv)
{
  try
  {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    (void)desc.add_options()
      ("help,h", "produce help message")
      ("backend,b",
       po::value<std::string>()->default_value("sdl-adweb42:9094,sdl-adweb43:9094"),
       "backend")
      ("thread_number,t", po::value<int>()->default_value(16), "thread number")
      ("tss,s", "testing ThriftPoolTss(testing ThriftPool by default)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
      std::cout << desc << std::endl;
      return 0;
    }

    std::string backend = vm["backend"].as<std::string>();
    int thread_number = vm["thread_number"].as<int>();
    bool tss = (vm.count("tss") != 0);

    if (tss)
    {
      boost::scoped_ptr<ThriftPoolTssType> pool;
      pool.reset(new ThriftPoolTssType(backend, true, 50, 50, 50));
      boost::this_thread::sleep(boost::posix_time::seconds(1));

      (void)install_signal_handler();

      printf("testing ThriftPoolTss\n");
      boost::thread_group group;
      for (int i=0; i<thread_number; i++)
        (void)group.create_thread(boost::bind(pressure_test_thread_tss, pool.get()));
      group.join_all();
      printf("finished testing ThriftPoolTss\n");
    }
    else
    {
      boost::scoped_ptr<ThriftPoolType> pool;
      pool.reset(new ThriftPoolType(backend, true, 50, 50, 50));
      boost::this_thread::sleep(boost::posix_time::seconds(1));

      (void)install_signal_handler();

      printf("testing ThriftPool\n");
      boost::thread_group group;
      for (int i=0; i<thread_number; i++)
        (void)group.create_thread(boost::bind(pressure_test_thread, pool.get()));
      group.join_all();
      printf("finished testing ThriftPool\n");
    }
  }
  catch (std::exception& e)
  {
    printf("caught: %s\n", e.what());
  }

  return 0;
}
