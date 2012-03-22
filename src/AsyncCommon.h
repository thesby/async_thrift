/** @file
* @brief common headers and type definitions
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_COMMON_H
#define ASYNC_COMMON_H

#include <assert.h>

#include <vector>
#include <string>
#include <sstream>
#include <exception>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/cstdint.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/thread.hpp>

#include <Thrift.h>
#include <TProcessor.h>
#include <TApplicationException.h>
#include <protocol/TProtocol.h>
#include <protocol/TBinaryProtocol.h>
#include <protocol/TProtocolException.h>
#include <server/TServer.h>
#include <transport/TBufferTransports.h>
#include <transport/TTransportException.h>

namespace apache { namespace thrift { namespace async {

  using boost::uint8_t;
  using boost::uint16_t;
  using boost::uint32_t;
  using boost::uint64_t;
  using boost::int8_t;
  using boost::int16_t;
  using boost::int32_t;
  using boost::int64_t;

  using ::apache::thrift::GlobalOutput;
  using ::apache::thrift::TProcessor;
  using ::apache::thrift::TException;
  using ::apache::thrift::TApplicationException;
  using ::apache::thrift::protocol::TProtocol;
  using ::apache::thrift::protocol::TBinaryProtocol;
  using ::apache::thrift::protocol::TProtocolException;
  using ::apache::thrift::server::TServer;
  using ::apache::thrift::transport::TMemoryBuffer;
  using ::apache::thrift::transport::TFramedTransport;
  using ::apache::thrift::transport::TTransportException;

  typedef boost::function<void (const boost::system::error_code& ec)> AsyncRPCCallback;
  typedef boost::function<void (const boost::system::error_code& ec, bool)> AsyncProcessorCallback;

} } } // namespace

#endif
