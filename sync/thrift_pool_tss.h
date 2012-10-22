/** @file
 * @brief thrift pool for thread specific storage
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef THRIFT_POOL_TSS_H
#define THRIFT_POOL_TSS_H

//lint -esym(578,index,socket) symbol hides symbol
//lint -esym(1712,ThriftConnectionTss,ThriftPoolTss) default constructor not defined
//lint -esym(1762,*::ThriftConnectionTss<*>::close) Member function could be made const
//lint -esym(1762,*::ThriftConnectionTss<*>::is_open) Member function could be made const

#include <host.h>
#include <stdlib.h>
#include <sys/time.h>
#include <limits>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/tss.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/random.hpp>
#include <Thrift.h>
#include <protocol/TBinaryProtocol.h>
#include <transport/TSocket.h>
#include <transport/TBufferTransports.h>

namespace apache { namespace thrift { namespace sync {

  using ::apache::thrift::GlobalOutput;

  template <class T>
    class ThriftConnectionTss
    {
      private:
        const std::string host_;
        const std::string resolved_host_;
        const int port_;
        const bool framed_transport_;
        const int thrift_conn_timeout_;
        const int thrift_send_timeout_;
        const int thrift_recv_timeout_;

        boost::shared_ptr< ::apache::thrift::transport::TTransport> transport_;
        boost::scoped_ptr<T> client_;


        bool inner_init(void)
        {
          boost::shared_ptr< ::apache::thrift::transport::TSocket>
            socket(new ::apache::thrift::transport::TSocket(resolved_host_, port_));

          socket->setConnTimeout(thrift_conn_timeout_);
          socket->setSendTimeout(thrift_send_timeout_);
          socket->setRecvTimeout(thrift_recv_timeout_);

          if (framed_transport_)
            transport_.reset(new ::apache::thrift::transport::TFramedTransport(socket));
          else
            transport_.reset(new ::apache::thrift::transport::TBufferedTransport(socket));

          boost::shared_ptr< ::apache::thrift::protocol::TBinaryProtocol>
            protocol(new ::apache::thrift::protocol::TBinaryProtocol(transport_));

          client_.reset(new T(protocol));

          return connect();
        }


      public:
        std::string get_host(void)const
        {
          return host_;
        }


        std::string get_resolved_host(void)const
        {
          return resolved_host_;
        }


        int get_port(void)const
        {
          return port_;
        }


        bool get_framed_transport(void)const
        {
          return framed_transport_;
        }


        ThriftConnectionTss(
            const std::string& host,
            const std::string& resolved_host,
            int port,
            bool framed_transport,
            int thrift_conn_timeout,
            int thrift_send_timeout,
            int thrift_recv_timeout)
          : host_(host),
          resolved_host_(resolved_host),
          port_(port),
          framed_transport_(framed_transport),
          thrift_conn_timeout_(thrift_conn_timeout),
          thrift_send_timeout_(thrift_send_timeout),
          thrift_recv_timeout_(thrift_recv_timeout),
          client_()
      {
        (void)inner_init();
      }


        ~ThriftConnectionTss()
        {
          close();
          client_.reset();
          transport_.reset();
        }


        ThriftConnectionTss * clone(void)const
        {
          return new ThriftConnectionTss(
              host_,
              resolved_host_,
              port_,
              framed_transport_,
              thrift_conn_timeout_,
              thrift_send_timeout_,
              thrift_recv_timeout_);
        }


        // never returns 0, but we need to check if 'is_open'
        T * client(void)
        {
          // connect and get client
          assure_connect();
          return client_.get();
        }


        bool is_open(void)
        {
          return transport_->isOpen();
        }


        bool connect(void)
        {
          close();

          try
          {
            transport_->open();
          }
          catch (const ::apache::thrift::TException& e)
          {
            GlobalOutput.printf("connect %s:%d: %s", host_.c_str(), port_, e.what());
            transport_->close();
            return false;
          }

          return true;
        }


        // try to connect if needed, but we need to check if 'is_open'
        void assure_connect(void)
        {
          if (!is_open())
            (void)connect();
        }


        void close(void)
        {
          try
          {
            transport_->close();
          }
          catch (...)
          {
          }
        }
    };


  /************************************************************************/
  template <class T>
    class ThriftPoolTss
    {
      public:
        typedef ThriftConnectionTss<T> ThriftConnectionType;


      private:
        typedef boost::uniform_int<size_t> UniformSizet;
        typedef boost::variate_generator<boost::mt19937&, UniformSizet> RandomType;
        boost::mt19937 mt_19937_;
        UniformSizet uniform_;
        RandomType rand_;

        const bool framed_transport_;
        const int thrift_conn_timeout_;
        const int thrift_send_timeout_;
        const int thrift_recv_timeout_;

        typedef std::vector<ThriftConnectionType *> PoolsType;
        PoolsType pools_;
        boost::thread_specific_ptr<size_t> index_;
        boost::thread_specific_ptr<ThriftConnectionType> connection_;


        void add_server(const std::string& host, int port)
        {
          std::string resolved_host;
          if (!resolve_host(host, &resolved_host))
            resolved_host = host;

          ThriftConnectionType * pool = new ThriftConnectionType(
              host,
              resolved_host,
              port,
              framed_transport_,
              thrift_conn_timeout_,
              thrift_send_timeout_,
              thrift_recv_timeout_);
          pools_.push_back(pool);
        }


        size_t& get_tss_index(void)
        {
          size_t * index = index_.get();
          if (index == 0)
          {
            index = new size_t;
            // to get a random number for the initial 'index'
            *index = get_random_number();
            index_.reset(index);
          }
          return *index;
        }


        size_t get_random_number(void)
        {
          return rand_();
        }

      public:
        ThriftPoolTss(
            const std::string& backends,
            bool framed_transport = true,
            int thrift_conn_timeout = 50,
            int thrift_send_timeout = 50,
            int thrift_recv_timeout = 50)
          : mt_19937_(0),
          uniform_(0, std::numeric_limits<size_t>::max()),
          rand_(mt_19937_, uniform_),
          framed_transport_(framed_transport),
          thrift_conn_timeout_(thrift_conn_timeout),
          thrift_send_timeout_(thrift_send_timeout),
          thrift_recv_timeout_(thrift_recv_timeout)
      {
        std::vector<std::string> split_vector;
        (void)boost::split(split_vector, backends, boost::is_any_of(","));
        for (size_t i=0; i<split_vector.size(); i++)
        {
          std::vector<std::string> host_port;
          (void)boost::split(host_port, split_vector[i], boost::is_any_of(":"));

          try
          {
            if (host_port.size() == 2)
              add_server(host_port[0], boost::lexical_cast<int>(host_port[1]));
          }
          catch (const std::exception& e)
          {
            GlobalOutput.printf("ThriftPoolTss::ThriftPoolTss() caught a std::exception: %s", e.what());
          }
          catch (...)
          {
            GlobalOutput("ThriftPoolTss::ThriftPoolTss() caught an unknown exception");
          }
        }
      }


        ~ThriftPoolTss()
        {
          for (size_t i=0; i<pools_.size(); i++)
            delete pools_[i];
          pools_.clear();
        }


        // put the return value, or delete it
        ThriftConnectionType * get(void)
        {
          size_t& index = get_tss_index();
          ThriftConnectionType * client;

          index = get_random_number();
          for (size_t i=0; i<3; i++)// hard code
          {
            client = pools_[(index + i) % pools_.size()]->clone();
            if (client->is_open())
              break;
            delete client;
            client = 0;
          }
          return client;
        }


        void put(ThriftConnectionType * client)
        {
          delete client;
        }


        ThriftConnectionType * get_tss(void)
        {
          ThriftConnectionType * client = connection_.get();
          if (client == 0 || !client->is_open())
          {
            client = get();
            connection_.reset(client);
          }

          return client;
        }


        void reset_tss(ThriftConnectionType * client = 0)
        {
          connection_.reset(client);
        }
    };

} } }

#endif
