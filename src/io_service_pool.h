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

#include <AsyncCommon.h>

namespace apache { namespace thrift { namespace async {

  void set_tss_io_service(boost::asio::io_service * ios);
  boost::asio::io_service * get_tss_io_service();
  void run_io_service_tss(boost::asio::io_service * ios);

  /// A pool of io_service objects.
  class io_service_pool : private boost::noncopyable
  {
  public:
    /// Construct the io_service pool.
    explicit io_service_pool(size_t pool_size);

    /// Run all io_service objects in the pool.
    void run();

    /// Stop all io_service objects in the pool.
    void stop();

    /// Get an io_service to use.
    boost::asio::io_service& get_io_service();

  private:
    typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
    typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

    /// The pool of io_services.
    std::vector<io_service_ptr> io_services_;

    /// The work that keeps the io_services running.
    std::vector<work_ptr> work_;

    /// The next io_service to use for a connection.
    size_t next_io_service_;
  };

} } } // namespace

#endif
