/** @file
 * @brief server performance benchmark
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef SERVER_BENCHMARK_H
#define SERVER_BENCHMARK_H

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <map>
#include <signal.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

namespace {

  bool g_stop_flag = false;

  extern "C" void signal_handler(int signo)
  {
    g_stop_flag = true;
  }

  bool install_signal_handler()
  {
    (void)signal(SIGINT, signal_handler);
    return true;
  }

  class ServerBenchmarkStat
  {
    public:
      ServerBenchmarkStat()
        :io_service_(), dump_timer_(io_service_),
        success_(0), failure_(0)
    {
      begin_ = boost::posix_time::microsec_clock::local_time();

      set_dump();

      io_service_.reset();
      dump_thread_.reset(new boost::thread
          (boost::bind(&boost::asio::io_service::run, &io_service_)));
    }

      ~ServerBenchmarkStat()
      {
        g_stop_flag = true;
        io_service_.stop();
        dump_thread_->join();
        dump_thread_.reset();

        dump_qps();
        dump_rtt();
      }

      void dump_qps(FILE * fp = stdout)
      {
        end_ = boost::posix_time::microsec_clock::local_time();

        boost::posix_time::time_duration td = end_ - begin_;
        int seconds = static_cast<int>(td.seconds());

        int64_t success_bak;
        int64_t failure_bak;
        {
          boost::mutex::scoped_lock guard(stat_mutex_);
          success_bak = success_;
          failure_bak = failure_;
        }

        double qps = ((double)(success_bak + failure_bak))/seconds;
        double successful_qps = ((double)(success_bak))/seconds;

        fprintf(fp, "Time cost: %d seconds\n", seconds);
        fprintf(fp, "Success: %lu\n", static_cast<unsigned long>(success_bak));
        fprintf(fp, "Failure: %lu\n", static_cast<unsigned long>(failure_bak));
        fprintf(fp, "Actual QPS: %f\n", qps);
        fprintf(fp, "Actual successful QPS: %f\n", successful_qps);
        fflush(fp);
      }

      void dump_rtt(FILE * fp = stdout)
      {
        std::map<int32_t, int64_t> rtt_map;

        {
          boost::mutex::scoped_lock guard(rtt_mutex_);
          rtt_map = rtt_map_;
        }

        uint64_t total = 0;
        uint64_t accumu_times = 0;
        std::map<int32_t, int64_t>::const_iterator first, last;

        first = rtt_map.begin();
        last = rtt_map.end();
        for (; first != last; ++first)
        {
          total += (*first).second;
        }

        fprintf(fp, "Total rtt statistics: %-10lu\n", total);

        first = rtt_map.begin();
        last = rtt_map.end();
        for (; first != last; ++first)
        {
          uint32_t ms = (*first).first;
          uint64_t times = (*first).second;
          accumu_times += times;

          fprintf(fp, "< %-5u ms, accumulative times: %-5lu, percentage: %%%4.2f, "
              "times: %-5lu, percentage: %%%4.2f\n",
              ms, accumu_times, accumu_times*100.0f/total, times, times*100.0f/total);
        }
        fflush(fp);
      }

      void inc_success()
      {
        boost::mutex::scoped_lock guard(stat_mutex_);
        success_++;
      }

      void inc_failure()
      {
        boost::mutex::scoped_lock guard(stat_mutex_);
        failure_++;
      }

      void inc_rtt(int32_t ms)
      {
        if (ms % 10 != 0)
        {
          ms = ms /10 * 10 + 10;
        }
        boost::mutex::scoped_lock guard(rtt_mutex_);
        rtt_map_[ms]++;
      }

      int64_t get_success()const
      {
        return success_;
      }

      int64_t get_failure()const
      {
        return failure_;
      }

      static ServerBenchmarkStat * instance()
      {
        static ServerBenchmarkStat obj;
        return &obj;
      }

    private:
      void dump()
      {
        dump_qps();
        dump_rtt();

        set_dump();
      }

      void set_dump()
      {
        dump_timer_.expires_from_now(boost::posix_time::seconds(1));
        dump_timer_.async_wait(boost::bind(&ServerBenchmarkStat::dump, this));
      }

      boost::posix_time::ptime begin_, end_;

      boost::scoped_ptr<boost::thread> dump_thread_;
      boost::asio::io_service io_service_;
      boost::asio::deadline_timer dump_timer_;

      // stat
      boost::mutex stat_mutex_;
      int64_t success_;
      int64_t failure_;

      // rtt
      boost::mutex rtt_mutex_;
      std::map<int32_t, int64_t> rtt_map_;
  };
}

#endif
