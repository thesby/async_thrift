/** @file
 * @brief host resolver
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include "host.h"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>


namespace apache { namespace thrift { namespace sync {

  bool resolve_host(const std::string& host, std::string * resolved)
  {
    struct addrinfo hints;
    struct addrinfo * result;
    int ret;
    char buf[128];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = static_cast<int>(SOCK_STREAM);
    hints.ai_protocol = IPPROTO_TCP;

    ret = getaddrinfo(host.c_str(), 0, &hints, &result);
    if (ret!=0)
      return false;

    if (result->ai_family == AF_INET)
      //lint --e(740) struct sockaddr_in6/struct sockaddr_in ptr conversion
      //lint --e(826) struct sockaddr_in6/struct sockaddr_in ptr conversion
      (void)inet_ntop(result->ai_family, (void *)&((struct sockaddr_in *)result->ai_addr)->sin_addr, buf, sizeof(buf));
    else
      //lint --e(740) struct sockaddr_in6/struct sockaddr_in ptr conversion
      //lint --e(826) struct sockaddr_in6/struct sockaddr_in ptr conversion
      (void)inet_ntop(result->ai_family, (void *)&((struct sockaddr_in6 *)result->ai_addr)->sin6_addr, buf, sizeof(buf));

    freeaddrinfo(result);

    (void)resolved->assign(buf);

    return true;
  }

} } }
