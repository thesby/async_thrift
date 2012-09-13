/** @file
 * @brief service base handler
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef SERVICE_BASE_HANDLER_H
#define SERVICE_BASE_HANDLER_H

#include "gen-cpp/Service.h"
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

namespace thrift_ext {

  class ServiceBaseHandler : virtual public ServiceIf
  {
    protected:
      ServiceStatus sstatus_;
      boost::mutex sstatus_rt_mutex_;
      boost::shared_ptr<ServiceStatusRT> sstatus_rt_;
      facebook::fb303::fb_status status_;

      ServiceBaseHandler(
          const std::string& group,
          const std::string& host,
          const std::string& service);
      virtual ~ServiceBaseHandler();

    public:
      /************************************************************************/
      /* from facebook::fb303::FacebookBase <- FacebookServiceIf */
      /************************************************************************/
      // null implemented
      virtual void getName(std::string& _return){}
      virtual void getVersion(std::string& _return){}
      virtual void getStatusDetails(std::string& _return){}
      virtual void getCounters(std::map<std::string, int64_t> & _return){}
      virtual int64_t getCounter(const std::string& key){return 0;}
      virtual void setOption(const std::string& key, const std::string& value){}
      virtual void getOption(std::string& _return, const std::string& key){}
      virtual void getOptions(std::map<std::string, std::string> & _return){}
      virtual void getCpuProfile(std::string& _return, const int32_t profileDurationInSec){}
      virtual int64_t aliveSince(){return 0;}
      virtual void reinitialize(){}

      // implemented
      virtual facebook::fb303::fb_status getStatus() {return status_;}
      virtual void shutdown();

      /************************************************************************/
      /* from thrift_ext.ServiceIf */
      /************************************************************************/
      // implemented
      virtual void get_status(ServiceStatus& _return) {_return = sstatus_;}
      virtual void get_status_rt(ServiceStatusRT& _return);

    public:
      void set_status(facebook::fb303::fb_status status) {status_ = status;}
      void set_status_rt(boost::shared_ptr<ServiceStatusRT>& sstatus_rt);
  };

} // namespace

#endif
