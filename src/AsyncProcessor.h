/** @file
* @brief base class for asynchronous processor
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_PROCOSSER_H
#define ASYNC_PROCOSSER_H

#include "AsyncCommon.h"

namespace apache { namespace thrift { namespace async {

  class AsyncProcessor
  {
  public:
    typedef ::apache::thrift::async::AsyncProcessorCallback AsyncProcessorCallback;

  protected:
    AsyncProcessor() {}

    virtual void process_fn(
      TProtocol * input_protocol,
      TProtocol * output_protocol,
      AsyncProcessorCallback callback,
      std::string& fname, int32_t seqid) = 0;

  public:
    virtual ~AsyncProcessor() {}

    //when the 'callback' is completed, 'out' shall be filled with stuffs.
    //if 'callback' is invoked with an 'error' code,
    //the connection at server side shall be disconnected.
    //if 'callback' is invoked with an 'success' code,
    //the connection at server side may be reused.
    //may throw
    virtual void process(
      boost::shared_ptr<TProtocol>& in,
      boost::shared_ptr<TProtocol>& out,
      AsyncProcessorCallback callback);
  };

} } }

#endif//ASYNC_PROCOSSER_H
