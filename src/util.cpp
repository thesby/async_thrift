/** @file
* @brief basic utilities
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <util.h>

namespace apache { namespace thrift { namespace async {

  std::string socket_address_to_string(
    const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket)
  {
    assert(socket);
    try
    {
      std::ostringstream oss;
      oss << "local:" << socket->local_endpoint() << " ";
      oss << "remote:" << socket->remote_endpoint();
      return oss.str();
    }
    catch (std::exception& e)
    {
      return e.what();
    }
  }

  std::string endpoint_to_string(const boost::asio::ip::tcp::endpoint& endpoint)
  {
    std::ostringstream oss;
    oss << endpoint;
    return oss.str();
  }

} } } // namespace
