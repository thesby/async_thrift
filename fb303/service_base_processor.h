/** @file
 * @brief service base processor
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef SERVICE_BASE_PROCESSOR_H
#define SERVICE_BASE_PROCESSOR_H

#include "service_base_handler.h"
#include "process_stat.h"
#include <time.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

namespace thrift_ext {

  template <class Processor, class Handler>
    class ServiceBaseProcessor : virtual public Processor
  {
    private:
      boost::shared_ptr<Handler> handler_;

      boost::asio::io_service io_service_;
      boost::scoped_ptr<boost::thread> update_thread_;
      boost::asio::deadline_timer update_timer_;

      boost::mutex sstatus_rt_mutex_;
      boost::shared_ptr<ServiceStatusRT> sstatus_rt_;

    private:
      void start_io_service()
      {
        if (!update_thread_)
        {
          set_update();

          io_service_.reset();
          update_thread_.reset(new boost::thread
              (boost::bind(&boost::asio::io_service::run, &io_service_)));
        }
      }

      void stop_io_service()
      {
        if (update_thread_)
        {
          io_service_.stop();
          update_thread_->join();
          update_thread_.reset();
        }
      }

      void set_update()
      {
        int cpu_usage;
        uint64_t memory_usage;

        cpu_usage = get_cpu_usage();
        get_memory_usage(&memory_usage, 0);

        {
          boost::mutex::scoped_lock guard(sstatus_rt_mutex_);
          ServiceStatusRT& rt = *sstatus_rt_;
          rt.__isset.cpu = true;
          rt.__isset.memory = true;
          rt.__isset.rpc_qps = true;
          rt.__isset.rpc_ok_qps = true;
          rt.__isset.rpc_cost = true;
          rt.cpu = cpu_usage;
          rt.memory = memory_usage;

          handler_->set_status_rt(sstatus_rt_);
          sstatus_rt_.reset(new ServiceStatusRT);
        }

        update_timer_.expires_from_now(boost::posix_time::seconds(1));
        update_timer_.async_wait(boost::bind(&ServiceBaseProcessor::set_update, this));
      }

    public:
      explicit ServiceBaseProcessor(const boost::shared_ptr<Handler>& handler)
        : Processor(handler),
        handler_(handler),
        io_service_(),
        update_timer_(io_service_)
    {
      sstatus_rt_.reset(new ServiceStatusRT);
      start_io_service();
    }

      virtual ~ServiceBaseProcessor()
      {
        stop_io_service();
      }

      virtual bool process_fn(
          ::apache::thrift::protocol::TProtocol * iprot,
          ::apache::thrift::protocol::TProtocol * oprot,
          std::string& fname,
          int32_t seqid)
      {
        struct timespec begin, end;
        clock_gettime(CLOCK_MONOTONIC, &begin);
        bool ok = Processor::process_fn(iprot, oprot, fname, seqid);
        clock_gettime(CLOCK_MONOTONIC, &end);

        int ms = (end.tv_sec - begin.tv_sec) * 1000 + (end.tv_nsec - begin.tv_nsec) / 1000000;
        ms = ms /10 * 10 + 10;// round up to multiples of 10 ms

        {
          boost::mutex::scoped_lock guard(sstatus_rt_mutex_);

          ServiceStatusRT& rt = *sstatus_rt_;

          // rpc
          rt.rpc_qps[fname]++;
          if (ok)
            rt.rpc_ok_qps[fname]++;

          rt.rpc_cost[fname][ms]++;
        }

        return ok;
      }
  };

} // namespace

#endif
