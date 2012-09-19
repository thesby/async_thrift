/** @file
 * @brief service base handler
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef SERVICE_BASE_HANDLER_H
#define SERVICE_BASE_HANDLER_H

#include <gen-cpp/Service.h>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <server/TServer.h>

//lint -esym(1712,ServiceBaseHandler) default constructor not defined
//lint -esym(1732,ServiceBaseHandler) no assignment operator
//lint -esym(1733,ServiceBaseHandler) no copy constructoro

namespace thrift_ext {

  class ServiceBaseHandler : virtual public ServiceIf
  {
    protected:
      ServiceStatus sstatus_;
      boost::mutex sstatus_rt_mutex_;
      boost::shared_ptr<ServiceStatusRT> sstatus_rt_;
      ::facebook::fb303::fb_status status_;

      boost::shared_ptr< ::apache::thrift::server::TServer> server_;

      ServiceBaseHandler(
          const std::string& group,
          const std::string& host,
          const std::string& service);
      virtual ~ServiceBaseHandler();

    public:
      /************************************************************************/
      /* from ::facebook::fb303::FacebookBase <- FacebookServiceIf */
      /************************************************************************/
      // null implemented
      virtual void getName(std::string&){}
      virtual void getVersion(std::string&){}
      virtual void getStatusDetails(std::string&){}
      virtual void getCounters(std::map<std::string, int64_t>&){}
      virtual int64_t getCounter(const std::string&){return 0;}
      virtual void setOption(const std::string&, const std::string&){}
      virtual void getOption(std::string&, const std::string&){}
      virtual void getOptions(std::map<std::string, std::string>&){}
      virtual void getCpuProfile(std::string&, const int32_t){}
      virtual int64_t aliveSince(){return 0;}
      virtual void reinitialize(){}

      // implemented
      virtual ::facebook::fb303::fb_status getStatus() {return status_;}
      virtual void shutdown();

      /************************************************************************/
      /* from thrift_ext.ServiceIf */
      /************************************************************************/
      // implemented
      virtual void get_status(ServiceStatus& _return) {_return = sstatus_;}
      virtual void get_status_rt(ServiceStatusRT& _return);

    public:
      void set_status(::facebook::fb303::fb_status status) {status_ = status;}
      void set_status_rt(boost::shared_ptr<ServiceStatusRT>& sstatus_rt);

    public:
      void set_server(const boost::shared_ptr< ::apache::thrift::server::TServer>& server) {server_ = server;}
  };

}

#endif
