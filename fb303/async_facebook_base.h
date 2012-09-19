/** @file
 * @brief an asynchronous fb303 base class
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef ASYNC_FACEBOOK_BASE_H
#define ASYNC_FACEBOOK_BASE_H

#include <gen-cpp/AsyncFacebookService.h>
#include <boost/thread.hpp>

namespace facebook { namespace fb303 {

  class AsyncFacebookBase
    : virtual public AsyncFacebookServiceIf
  {
    private:
      const std::string name_;
      const std::string version_;
      fb_status status_;
      std::string status_details_;
      const int64_t alive_since_;
      boost::shared_ptr< ::apache::thrift::server::TServer> server_;

    protected:
      boost::mutex counters_mutex_;
      std::map<std::string, int64_t> counters_;
      boost::mutex options_mutex_;
      std::map<std::string, std::string> options_;

    public:
      std::string get_name()const
      {
        return name_;
      }

      std::string get_version()const
      {
        return version_;
      }

      void set_status(fb_status status)
      {
        status_ = status;
      }

      fb_status get_status()const
      {
        return status_;
      }

      void set_status_details(const std::string& status_details)
      {
        status_details_ = status_details;
      }

      std::string get_status_details()const
      {
        return status_details_;
      }

      int64_t get_alive_since()const
      {
        return alive_since_;
      }

      void set_server(const boost::shared_ptr< ::apache::thrift::server::TServer>& server)
      {
        server_ = server;
      }

    public:
      AsyncFacebookBase();
      explicit AsyncFacebookBase(const std::string& name, const std::string& version);
      virtual ~AsyncFacebookBase();

      virtual void async_getName(std::string& _return, ::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_getVersion(std::string& _return, ::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_getStatus(fb_status& _return, ::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_getStatusDetails(std::string& _return, ::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_getCounters(std::map<std::string, int64_t> & _return, ::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_getCounter(int64_t& _return, const std::string& key, ::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_setOption(const std::string& key, const std::string& value, ::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_getOption(std::string& _return, const std::string& key, ::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_getOptions(std::map<std::string, std::string> & _return, ::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_getCpuProfile(std::string& _return, const int32_t profileDurationInSec, ::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_aliveSince(int64_t& _return, ::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_reinitialize(::apache::thrift::async::AsyncRPCCallback callback);
      virtual void async_shutdown(::apache::thrift::async::AsyncRPCCallback callback);
  };

} }

#endif
