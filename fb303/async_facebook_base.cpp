/** @file
 * @brief an asynchronous fb303 base class
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include "async_facebook_base.h"
#include <time.h>

namespace facebook { namespace fb303 {

  AsyncFacebookBase::AsyncFacebookBase()
    : name_("fb303"),// hard code
    version_("0.1.0"),// hard code
    alive_since_(static_cast<int64_t>(time(0)))
  {
    status_ = STOPPED;
  }

  AsyncFacebookBase::AsyncFacebookBase(const std::string& name, const std::string& version)
    : name_(name), version_(version), alive_since_(static_cast<int64_t>(time(0)))
  {
    status_ = STOPPED;
  }

  AsyncFacebookBase::~AsyncFacebookBase()
  {
  }

  void AsyncFacebookBase::async_getName(std::string& _return,
      ::apache::thrift::async::AsyncRPCCallback callback)
  {
    _return = name_;
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_getVersion(std::string& _return,
      ::apache::thrift::async::AsyncRPCCallback callback)
  {
    _return = version_;
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_getStatus(fb_status& _return,
      ::apache::thrift::async::AsyncRPCCallback callback)
  {
    _return = status_;
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_getStatusDetails(std::string& _return,
      ::apache::thrift::async::AsyncRPCCallback callback)
  {
    _return = status_details_;
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_getCounters(std::map<std::string, int64_t> & _return,
      ::apache::thrift::async::AsyncRPCCallback callback)
  {
    boost::mutex::scoped_lock guard(counters_mutex_);
    _return = counters_;
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_getCounter(int64_t& _return, const std::string& key,
      ::apache::thrift::async::AsyncRPCCallback callback)
  {
    boost::mutex::scoped_lock guard(counters_mutex_);
    _return = counters_[key];
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_setOption(const std::string& key, const std::string& value,
      ::apache::thrift::async::AsyncRPCCallback callback)
  {
    boost::mutex::scoped_lock guard(options_mutex_);
    options_[key] = value;
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_getOption(std::string& _return, const std::string& key,
      ::apache::thrift::async::AsyncRPCCallback callback)
  {
    boost::mutex::scoped_lock guard(options_mutex_);
    _return = options_[key];
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_getOptions(std::map<std::string, std::string> & _return,
      ::apache::thrift::async::AsyncRPCCallback callback)
  {
    boost::mutex::scoped_lock guard(options_mutex_);
    _return = options_;
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_getCpuProfile(std::string& /*_return*/, const int32_t /*profileDurationInSec*/,
      ::apache::thrift::async::AsyncRPCCallback callback)
  {
    // do nothing
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_aliveSince(int64_t& _return,
      ::apache::thrift::async::AsyncRPCCallback callback)
  {
    _return = alive_since_;
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_reinitialize(::apache::thrift::async::AsyncRPCCallback callback)
  {
    // do nothing
    callback(boost::system::error_code());
  }

  void AsyncFacebookBase::async_shutdown(::apache::thrift::async::AsyncRPCCallback callback)
  {
    if (server_)
    {
      server_->stop();
      server_.reset();
    }
    callback(boost::system::error_code());
  }

} }
