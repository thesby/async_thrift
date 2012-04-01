/** @file
* @brief basic utilities
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <util.h>

namespace apache { namespace thrift { namespace async {

  std::string dump_address(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket)
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

} } } // namespace
