#!/usr/local/bin/thrift --java --cpp --py
include "base.thrift"

namespace cpp com.langtaojin.adgaga

# The API version (NOT the product version), composed as a dot delimited
# string with major, minor, and patch level components.
#
#  - Major: Incremented for backward incompatible changes. An example would
#           be changes to the number or disposition of method arguments.
#  - Minor: Incremented for backward compatible changes. An example would
#           be the addition of a new (optional) method.
#  - Patch: Incremented for bug fixes. The patch level should be increased
#           for every edit that doesn't result in a change to major/minor.
#
# See the Semantic Versioning Specification (SemVer) http://semver.org.
const string VERSION = "0.1.0"


struct Request {
  1: optional string message,
}


struct Response {
  1: optional string message,
}


struct TestStruct {
  1: optional i32 f1,
  2: optional i64 f2,
  3: optional double f3,
  4: optional string f4,
  5: required i32 f5,
  6: required i64 f6,
  7: required double f7,
  8: required string f8,

  9: optional list<i32> f9,
  10: optional list<string> f10,
  11: optional list<Request> f11,

  12: optional set<i32> f12,
  13: optional set<double> f13,
  14: optional set<string> f14,

  15: optional map<i32, i32> f15,
  16: optional map<double, Response> f16,
  17: optional map<string, string> f17,
}


service EchoServer extends base.BaseServer {
  Response echo(1:required Request request),
  i32 echo2(1:required i32 i),
  string echo3(1:required string str),
  string echo4(1:required i32 i1 2:required i64 i2),

  void void_func(),
  void void_func2(1:required Request request 2:required string str),
  oneway void oneway_func(),
  oneway void oneway_func2(1:required Request request 2:required string str),
}

