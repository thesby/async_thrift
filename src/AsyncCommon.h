/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_COMMON_H
#define ASYNC_COMMON_H

#include <assert.h>
#include <stdint.h>

#include <vector>
#include <string>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread.hpp>
#include <boost/weak_ptr.hpp>

#include <Thrift.h>
#include <TProcessor.h>
#include <protocol/TProtocol.h>
#include <protocol/TBinaryProtocol.h>
#include <server/TServer.h>
#include <transport/TBufferTransports.h>
#include <transport/TTransportException.h>

namespace apache { namespace thrift { namespace async {

  using ::apache::thrift::GlobalOutput;
  using ::apache::thrift::TProcessor;
  using ::apache::thrift::protocol::TProtocol;
  using ::apache::thrift::protocol::TBinaryProtocol;
  using ::apache::thrift::server::TServer;
  using ::apache::thrift::transport::TMemoryBuffer;
  using ::apache::thrift::transport::TFramedTransport;
  using ::apache::thrift::transport::TTransportException;

  typedef boost::function<void (const boost::system::error_code& ec)> AsyncRPCCallback;
  typedef boost::function<void (const boost::system::error_code& ec)> AsyncProcessorCallback;

} } } // namespace

#endif
