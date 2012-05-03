/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <stdio.h>
#include "gen-cpp/test_types.h"

using namespace ::com::langtaojin::adgaga;

int main(int argc, char **argv)
{
  TestStruct test;
  Request req;
  Response res;
  std::string s;

  test.__isset.f1 = true;
  test.f1 = 1;
  test.__isset.f2 = false;
  test.f2 = 2;
  test.__isset.f3 = true;
  test.f3 = 3.1415926;
  test.__isset.f4 = true;
  test.f4 = "3.1415926";

  test.f5 = 5;
  test.f6 = 6;
  test.f7 = 7.777;
  test.f8 = "8.888";

  test.__isset.f9 = true;
  test.f9.push_back(1);
  test.f9.push_back(10);
  test.f9.push_back(100);
  test.f9.push_back(1000);

  test.__isset.f10 = true;
  test.f10.push_back("1");
  test.f10.push_back("2");

  test.__isset.f11 = true;
  req.__isset.message = true;
  req.message = "f11 message";
  test.f11.push_back(req);
  req.message = "f11 message 2";
  test.f11.push_back(req);
  req.message = "f11 message 3";
  test.f11.push_back(req);

  test.__isset.f12 = true;
  test.f12.insert(1);
  test.f12.insert(10);
  test.f12.insert(100);
  test.f12.insert(1000);

  test.__isset.f13 = true;
  test.f13.insert(1);
  test.f13.insert(10);
  test.f13.insert(100);
  test.f13.insert(1000);

  test.__isset.f14 = true;
  test.f14.insert("1");
  test.f14.insert("10");
  test.f14.insert("100");
  test.f14.insert("1000");

  test.__isset.f15 = true;
  test.f15.insert(std::make_pair(1,1));
  test.f15.insert(std::make_pair(10,10));
  test.f15.insert(std::make_pair(100,100));
  test.f15.insert(std::make_pair(1000,1000));

  res.__isset.message = true;
  res.message = "f16 message";
  test.__isset.f16 = true;
  test.f16.insert(std::make_pair(1.0,res));
  test.f16.insert(std::make_pair(10.0,res));
  test.f16.insert(std::make_pair(100.0,res));
  test.f16.insert(std::make_pair(1000.0,res));

  test.__isset.f17 = true;
  test.f17.insert(std::make_pair("1","1"));
  test.f17.insert(std::make_pair("10","10"));
  test.f17.insert(std::make_pair("100","100"));
  test.f17.insert(std::make_pair("1000","1000"));

  test.to_string(&s);
  printf("TestStruct=\n%s\n", s.c_str());

  return 0;
}
