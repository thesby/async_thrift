/** @file
* @brief exceptions
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_EXCEPTION_H
#define ASYNC_EXCEPTION_H

#include <AsyncCommon.h>

namespace apache { namespace thrift { namespace async {

  //The following thrift exceptions are mapped to boost::system::error_code
  //to be passed to various handlers:
  //  ::apache::thrift::TException
  //  ::apache::thrift::TApplicationException
  //  ::apache::thrift::protocol::TProtocolException
  //  ::apache::thrift::transport::TTransportException

  enum kTException
  {
    kThriftUnknown = 0,
    //The following kinds of exceptions are defined by async-thrift
    kThriftHasPendingOp = 1,
  };

  enum kTApplicationException
  {
    kAppUnknown = TApplicationException::UNKNOWN,
    kAppMethod = TApplicationException::UNKNOWN_METHOD,
    kAppInvalidMessageType = TApplicationException::INVALID_MESSAGE_TYPE,
    kAppWrongMethodName = TApplicationException::WRONG_METHOD_NAME,
    kAppBadSequenceID = TApplicationException::BAD_SEQUENCE_ID,
    kAppMissingResult = TApplicationException::MISSING_RESULT,
  };

  enum kTProtocolException
  {
    kProtoUnknown = TProtocolException::UNKNOWN,
    kProtoInvalidData = TProtocolException::INVALID_DATA,
    kProtoNegativeSize = TProtocolException::NEGATIVE_SIZE,
    kProtoSizeLimit = TProtocolException::SIZE_LIMIT,
    kProtoBadVersion = TProtocolException::BAD_VERSION,
    kProtoNotImplemented = TProtocolException::NOT_IMPLEMENTED,
  };

  enum kTTransportException
  {
    kTransUnknown = TTransportException::UNKNOWN,
    kTransNotOpen = TTransportException::NOT_OPEN,
    kTransTimedOut = TTransportException::TIMED_OUT,
    kTransEndOfFile = TTransportException::END_OF_FILE,
    kTransInterrupted = TTransportException::INTERRUPTED,
    kTransBadArgs = TTransportException::BAD_ARGS,
    kTransCorruptedData = TTransportException::CORRUPTED_DATA,
    kTransInternalError = TTransportException::INTERNAL_ERROR,
  };


  const boost::system::error_category& get_thrift_category();
  const boost::system::error_category& get_application_category();
  const boost::system::error_category& get_protocol_category();
  const boost::system::error_category& get_transport_category();

  inline boost::system::error_code make_error_code(kTException code)
  {
    return boost::system::error_code(code,
      get_thrift_category());
  }

  inline boost::system::error_code make_error_code(
    const TException&)
  {
    return boost::system::error_code(
      static_cast<int>(kThriftUnknown), get_thrift_category());
  }

  inline boost::system::error_code make_error_code(kTApplicationException code)
  {
    return boost::system::error_code(code,
      get_application_category());
  }

  inline boost::system::error_code make_error_code(
    const TApplicationException& e)
  {
    return boost::system::error_code(
      static_cast<int>(
      // ugly: getType should be a const method
      const_cast<TApplicationException&>(e).getType()),
      get_application_category());
  }

  inline boost::system::error_code make_error_code(kTProtocolException code)
  {
    return boost::system::error_code(code,
      get_protocol_category());
  }

  inline boost::system::error_code make_error_code(
    const TProtocolException& e)
  {
    return boost::system::error_code(
      static_cast<int>(
      // ugly: getType should be a const method
      const_cast<TProtocolException&>(e).getType()),
      get_protocol_category());
  }

  inline boost::system::error_code make_error_code(kTTransportException code)
  {
    return boost::system::error_code(code,
      get_transport_category());
  }

  inline boost::system::error_code make_error_code(
    const TTransportException& e)
  {
    return boost::system::error_code(
      static_cast<int>(e.getType()), get_transport_category());
  }

} } } // namespace

#endif
