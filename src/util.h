/** @file
* @brief basic utilities
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#ifndef UTIL_H
#define UTIL_H

#include <async_common.h>

namespace apache { namespace thrift { namespace async {

  std::string dump_address(
    const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket);

} } } // namespace

#endif
