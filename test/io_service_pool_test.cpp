/** @file
 * @brief io_service pool test
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include <io_service_pool.h>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

using namespace ::apache::thrift::async;

struct Counter
{
  unsigned times;
  unsigned loop;
  boost::mutex mutex;
};

void on_accept(boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr,
    const boost::asio::ip::tcp::endpoint& endpoint,
    Counter * counter,
    IOServicePool * pool,
    const boost::system::error_code& ec)
{
  printf("on_accept %s %u %u\n",
      ec.message().c_str(), counter->times, counter->loop);

  sock_ptr->close();

  bool new_loop;
  bool should_end = false;
  counter->mutex.lock();
  new_loop = (--counter->times == 0);
  if (new_loop)
  {
    counter->times = 16;
    should_end = (--counter->loop == 0);
  }
  counter->mutex.unlock();

  if (new_loop)
  {
    pool->stop();

    if (!should_end)
    {
      sock_ptr->async_connect(
          endpoint, boost::bind(&on_accept, sock_ptr, endpoint, counter, pool, _1));
      pool->run();
    }
  }
  else
  {
    sock_ptr->async_connect(
        endpoint, boost::bind(&on_accept, sock_ptr, endpoint, counter, pool, _1));
  }
}

int main(int argc, char * argv[])
{
  IOServicePool pool(16);
  Counter counter;
  counter.times = 16;
  counter.loop = 16;

  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), 12500);

  boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr
    (new boost::asio::ip::tcp::socket(pool.get_io_service()));
  sock_ptr->async_connect(
      endpoint, boost::bind(&on_accept, sock_ptr, endpoint, &counter, &pool, _1));

  pool.run();

  return 0;
}
