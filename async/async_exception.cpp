/** @file
 * @brief exceptions
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include "async_exception.h"

namespace apache { namespace thrift { namespace async {

  namespace {

    class thrift_category : public boost::system::error_category
    {
      public:
        const char * name() const
        {
          return "ThriftException";
        }

        std::string message(int value) const
        {
          switch (value)
          {
            case kThriftHasPendingOp:
              return "Thrift: There are pending asynchronous operations";
            case kThriftUnknown:
            default:
              return "Thrift: Thrift exception";
          }
        }
    };

    class application_category : public boost::system::error_category
    {
      public:
        const char * name() const
        {
          return "TApplicationException";
        }

        std::string message(int value) const
        {
          switch (value)
          {
            case kAppUnknown:
              return "TApplicationException: Unknown application exception";
            case kAppMethod:
              return "TApplicationException: Unknown method";
            case kAppInvalidMessageType:
              return "TApplicationException: Invalid message type";
            case kAppWrongMethodName:
              return "TApplicationException: Wrong method name";
            case kAppBadSequenceID:
              return "TApplicationException: Bad sequence identifier";
            case kAppMissingResult:
              return "TApplicationException: Missing result";
            default:
              return "TApplicationException: (Invalid exception type)";
          }
        }
    };

    class protocol_category : public boost::system::error_category
    {
      public:
        const char * name() const
        {
          return "TProtocolException";
        }

        std::string message(int value) const
        {
          switch (value)
          {
            case kProtoUnknown:
              return "TProtocolException: Unknown protocol exception";
            case kProtoInvalidData:
              return "TProtocolException: Invalid data";
            case kProtoNegativeSize:
              return "TProtocolException: Negative size";
            case kProtoSizeLimit:
              return "TProtocolException: Exceeded size limit";
            case kProtoBadVersion:
              return "TProtocolException: Invalid version";
            case kProtoNotImplemented:
              return "TProtocolException: Not implemented";
            default:
              return "TProtocolException: (Invalid exception type)";
          }
        }
    };

    class transport_category : public boost::system::error_category
    {
      public:
        const char * name() const
        {
          return "TTransportException";
        }

        std::string message(int value) const
        {
          switch (value)
          {
            case kTransUnknown:
              return "TTransportException: Unknown transport exception";
            case kTransNotOpen:
              return "TTransportException: Transport not open";
            case kTransTimedOut:
              return "TTransportException: Timed out";
            case kTransEndOfFile:
              return "TTransportException: End of file";
            case kTransInterrupted:
              return "TTransportException: Interrupted";
            case kTransBadArgs:
              return "TTransportException: Invalid arguments";
            case kTransCorruptedData:
              return "TTransportException: Corrupted Data";
            case kTransInternalError:
              return "TTransportException: Internal error";
            default:
              return "TTransportException: (Invalid exception type)";
          }
        }
    };
  }

  const boost::system::error_category& get_thrift_category()
  {
    //lint -e(1502) 'thrift_category' has no nonstatic data members
    static thrift_category instance;
    return instance;
  }

  const boost::system::error_category& get_application_category()
  {
    //lint -e(1502) 'thrift_category' has no nonstatic data members
    static application_category instance;
    return instance;
  }

  const boost::system::error_category& get_protocol_category()
  {
    //lint -e(1502) 'thrift_category' has no nonstatic data members
    static protocol_category instance;
    return instance;
  }

  const boost::system::error_category& get_transport_category()
  {
    //lint -e(1502) 'thrift_category' has no nonstatic data members
    static transport_category instance;
    return instance;
  }

} } }
