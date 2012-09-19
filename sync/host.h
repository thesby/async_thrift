/** @file
 * @brief host resolver
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef HOST_H
#define HOST_H

#include <string>

namespace apache { namespace thrift { namespace sync {

  bool resolve_host(const std::string& host, std::string * resolved);

} } }

#endif
