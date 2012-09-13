/** @file
 * @brief service base handler
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include "service_base_handler.h"

namespace thrift_ext {

  ServiceBaseHandler::ServiceBaseHandler(
      const std::string& group,
      const std::string& host,
      const std::string& service)
  {
    sstatus_.__isset.group = true;
    sstatus_.__isset.host = true;
    sstatus_.__isset.service_ = true;
    sstatus_.group = group;
    sstatus_.host = host;
    sstatus_.service_ = service;

    sstatus_rt_.reset(new ServiceStatusRT);

    set_status(facebook::fb303::ALIVE);
  }

  ServiceBaseHandler::~ServiceBaseHandler()
  {
    set_status(facebook::fb303::DEAD);
  }

  void ServiceBaseHandler::shutdown()
  {
    set_status(facebook::fb303::STOPPED);
  }

  void ServiceBaseHandler::get_status_rt(ServiceStatusRT& _return)
  {
    boost::mutex::scoped_lock guard(sstatus_rt_mutex_);
    _return = *sstatus_rt_;
  }

  void ServiceBaseHandler::set_status_rt(boost::shared_ptr<ServiceStatusRT>& sstatus_rt)
  {
    boost::mutex::scoped_lock guard(sstatus_rt_mutex_);
    sstatus_rt_.swap(sstatus_rt);
  }

} // namespace
