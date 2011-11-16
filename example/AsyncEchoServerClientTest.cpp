/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include "AsyncEchoServerClient.h"

using namespace com::langtaojin::adgaga;

void echo_callback(AsyncEchoServerClient * client,
                   Response * _return, Request * request, size_t * times,
                   const boost::system::error_code& ec)
{
  assert(client && _return && request);

  if (!ec)
  {
    printf("async_echo callback: %s\n", _return->message.c_str());
    //request->message += " ^_^";

    if (--(*times) != 0)
      client->async_echo(*_return, *request,
      boost::bind(echo_callback, client, _return, request, times, _1));
  }
  else
  {
    printf("%s %s\n", __FUNCTION__, ec.message().c_str());
  }
}

struct Session
{
  boost::shared_ptr<AsyncEchoServerClient> client;
  Response _return;
  Request request;
  size_t times;
};

int main()
{
  try
  {
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), 12500);

    Session session[32];
    for (size_t i=0; i<sizeof(session)/sizeof(session[0]); i++)
    {
      boost::shared_ptr<boost::asio::ip::tcp::socket>
        socket(new boost::asio::ip::tcp::socket(io_service));

      socket->connect(endpoint);

      session[i].client.reset(new AsyncEchoServerClient(socket));
      session[i].client->set_rpc_timeout(50);
      session[i].times = 1024;
      session[i].request.__isset.message = true;
      session[i].request.message = "Hello World";

      session[i].client->async_echo(session[i]._return, session[i].request,
        boost::bind(echo_callback, session[i].client.get(),
        &session[i]._return, &session[i].request, &session[i].times, _1));
    }

    boost::thread_group tg;
    for (size_t i=0; i<4; i++)
      tg.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
    tg.join_all();
  }
  catch (std::exception& e)
  {
    printf("caught: %s\n", e.what());
  }
  catch (...)
  {
    printf("caught: something\n");
  }
  return 0;
}
