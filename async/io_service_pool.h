/** @file
 * @brief io_service pool
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef IO_SERVICE_POOL_H
#define IO_SERVICE_POOL_H

#include <vector>
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

//lint -esym(1712,io_service_pool) default constructor not defined
//lint -esym(1732,io_service_pool) no assignment operator
//lint -esym(1733,io_service_pool) no copy constructor

namespace apache { namespace thrift { namespace async {

  void set_tss_io_service(boost::asio::io_service * ios);
  boost::asio::io_service * get_tss_io_service();
  void run_io_service_tss(boost::asio::io_service * ios);

  // This piece of code is derived from asio example
  class io_service_pool : private boost::noncopyable
  {
    public:
      explicit io_service_pool(size_t pool_size);
      ~io_service_pool() {}
      // When enable_tss is true,
      // run_io_service_tss rather than boost::asio::io_service::run is the thread function.
      // At this time,
      // we can use get_tss_io_service to obtain the io_service bound to the current thread.
      void run(bool enable_tss = true);
      void stop();
      // Use a round-robin scheme to choose the next io_service to use.
      boost::asio::io_service& get_io_service();
      // Choose a io_service by index of inner array
      boost::asio::io_service& get_io_service(size_t index);

      size_t size()const
      {
        return io_services_.size();
      }

    private:
      typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
      typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

      std::vector<io_service_ptr> io_services_;
      std::vector<work_ptr> work_;
      size_t next_io_service_;
      boost::mutex mutex_;
  };

  // make the convention of class name uniform
  typedef io_service_pool IOServicePool;

} } } // namespace

#endif
