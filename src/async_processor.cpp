/** @file
* @brief base class for asynchronous processor
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <async_processor.h>
#include <async_exception.h>

namespace apache { namespace thrift { namespace async {

  void AsyncProcessor::process(
    boost::shared_ptr<TProtocol>& pinput_protocol,
    boost::shared_ptr<TProtocol>& poutput_protocol,
    AsyncProcessorCallback callback)
  {
    TProtocol * input_protocol = pinput_protocol.get();
    TProtocol * output_protocol = poutput_protocol.get();
    std::string fname;
    ::apache::thrift::protocol::TMessageType mtype;
    int32_t seqid;

    input_protocol->readMessageBegin(fname, mtype, seqid);

    if (mtype != ::apache::thrift::protocol::T_CALL
      && mtype != ::apache::thrift::protocol::T_ONEWAY)
    {
      input_protocol->skip(::apache::thrift::protocol::T_STRUCT);
      input_protocol->readMessageEnd();
      input_protocol->getTransport()->readEnd();
      TApplicationException x(TApplicationException::INVALID_MESSAGE_TYPE);
      output_protocol->writeMessageBegin(fname,
        ::apache::thrift::protocol::T_EXCEPTION, seqid);
      x.write(output_protocol);
      output_protocol->writeMessageEnd();
      output_protocol->getTransport()->flush();
      output_protocol->getTransport()->writeEnd();

      //invoke callback
      //Here is_oneway(the second parameter 'false') is ignored,
      //because error code is specified
      callback(make_error_code(x), false);
      return;
    }
    process_fn(input_protocol, output_protocol, callback, fname, seqid);
  }

} } }
