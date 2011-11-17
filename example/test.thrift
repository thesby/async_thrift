#!/usr/local/bin/thrift --java --cpp --py

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


service EchoServer {
  Response echo(1:required Request request),
  i32 echo2(1:required i32 i),
  string echo3(1:required string str),
  string echo4(1:required i32 i1 2:required i64 i2),

  void void_func(),
  oneway void oneway_func(),
}

