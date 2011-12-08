/** @file
* @brief
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
  protected:
    AsyncProcessor() {}
  public:
    virtual ~AsyncProcessor() {}

    //when the 'callback' is invoked, 'out' shall be filled with stuffs
    virtual void process(
      AsyncProcessorCallback callback,
      boost::shared_ptr<TProtocol> in,
      boost::shared_ptr<TProtocol> out) = 0;

    virtual void process(
      AsyncProcessorCallback callback,
      boost::shared_ptr<TProtocol> io)
    {
      return process(callback, io, io);
    }
  };

} } }

#endif//ASYNC_PROCOSSER_H
