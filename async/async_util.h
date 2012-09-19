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

//lint -esym(578,socket) symbol hides symbol

namespace apache { namespace thrift { namespace async {

  std::string socket_address_to_string(
      const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket);
  std::string endpoint_to_string(const boost::asio::ip::tcp::endpoint& endpoint);

} } } // namespace

#endif
