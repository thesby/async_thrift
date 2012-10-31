include "fb303.thrift"

namespace java thrift_ext
namespace cpp thrift_ext
namespace py thrift_ext

struct ServiceStatus
{
  1: optional string group;
  2: optional string host;
  3: optional string service_;
}

struct ServiceStatusRT
{
  1: optional i32 cpu;
  2: optional i64 memory;
  3: optional map<string, i32> rpc_qps;// rpc name to qps map
  4: optional map<string, i32> rpc_ok_qps;// rpc name to successful qps map
  5: optional map<string, map<i32, i64>> rpc_cost;// rpc name to (ms to times) map
  6: optional map<string, map<string,i64>> error;// error to times map
}

service Service extends fb303.FacebookService
{
  ServiceStatus get_status();
  ServiceStatusRT get_status_rt();
}

