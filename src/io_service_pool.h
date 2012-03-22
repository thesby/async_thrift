/** @file
* @brief io_service pool
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2011 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef IO_SERVICE_POOL_H
#define IO_SERVICE_POOL_H

#include <vector>
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

namespace apache { namespace thrift { namespace async {

  void set_tss_io_service(boost::asio::io_service * ios);
  boost::asio::io_service * get_tss_io_service();
  void run_io_service_tss(boost::asio::io_service * ios);

  // This piece of code is from asio example, and the naming convention remains
  class io_service_pool : private boost::noncopyable
  {
  public:
    io_service_pool(size_t pool_size);
    void run(bool enable_tss = true);
    void stop();
    // Use a round-robin scheme to choose the next io_service to use.
    boost::asio::io_service& get_io_service();

  private:
    typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
    typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

    std::vector<io_service_ptr> io_services_;
    std::vector<work_ptr> work_;
    size_t next_io_service_;
  };

  // make the convention of class name comply
  typedef io_service_pool IOServicePool;

} } } // namespace

#endif
