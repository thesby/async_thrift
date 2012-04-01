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
#include <io_service_pool.h>
#include <boost/thread/tss.hpp>
#include <boost/thread/thread.hpp>

namespace apache { namespace thrift { namespace async {

  static boost::thread_specific_ptr<boost::asio::io_service *> s_tss_io_service;

  void set_tss_io_service(boost::asio::io_service * ios)
  {
    boost::asio::io_service ** ptr = new boost::asio::io_service *(ios);
    s_tss_io_service.reset(ptr);
  }

  boost::asio::io_service * get_tss_io_service()
  {
    return *s_tss_io_service.get();
  }

  void run_io_service_tss(boost::asio::io_service * ios)
  {
    set_tss_io_service(ios);
    ios->run();
  }

  io_service_pool::io_service_pool(size_t pool_size)
    : next_io_service_(0)
  {
    if (pool_size == 0)
      pool_size = 1;

    for (size_t i=0; i<pool_size; ++i)
    {
      io_service_ptr io_service(new boost::asio::io_service);
      io_services_.push_back(io_service);
    }
  }

  void io_service_pool::run(bool enable_tss)
  {
    for (size_t i=0; i<io_services_.size(); ++i)
    {
      work_ptr work(new boost::asio::io_service::work(*io_services_[i]));
      work_.push_back(work);
      io_services_[i]->reset();
    }
    next_io_service_ = 0;

    boost::thread_group tg;
    for (size_t i=0; i<io_services_.size(); i++)
    {
      if (enable_tss)
        tg.create_thread(
          boost::bind(run_io_service_tss, io_services_[i].get()));
      else
        tg.create_thread(
          boost::bind(&boost::asio::io_service::run, io_services_[i].get()));
    }

    tg.join_all();
  }

  void io_service_pool::stop()
  {
    work_.clear();
    for (size_t i=0; i<io_services_.size(); ++i)
      io_services_[i]->stop();
    next_io_service_ = 0;
  }

  boost::asio::io_service& io_service_pool::get_io_service()
  {
    boost::asio::io_service& io_service = *io_services_[next_io_service_];
    ++next_io_service_;
    if (next_io_service_ == io_services_.size())
      next_io_service_ = 0;
    return io_service;
  }

  boost::asio::io_service& io_service_pool::get_io_service(size_t index)
  {
    return *io_services_[index % io_services_.size()];
  }

} } } // namespace
