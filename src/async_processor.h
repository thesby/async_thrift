/** @file
* @brief base class for asynchronous processor
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef ASYNC_PROCOSSER_H
#define ASYNC_PROCOSSER_H

#include <async_common.h>

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

    //When the 'callback' is completed, 'out' shall be filled with stuffs.
    //If 'callback' is invoked with an 'erroneous' code,
    //the connection at server side shall be disconnected.
    //If 'callback' is invoked with a 'successful' code,
    //the connection at server side may be reused.
    virtual void process(
      boost::shared_ptr<TProtocol>& in,
      boost::shared_ptr<TProtocol>& out,
      AsyncProcessorCallback callback);//may throw
  };

} } }

#endif//ASYNC_PROCOSSER_H
