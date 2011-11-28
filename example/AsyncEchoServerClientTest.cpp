/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <vector>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
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
    //printf("async_echo callback: %s\n", _return->message.c_str());
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

int main(int argc, char **argv)
{
  try
  {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "produce help message")
      ("port,p", po::value<int>()->default_value(12500), "listening port")
      ("clients,c", po::value<int>()->default_value(768), "clients' number")
      ("timeout", po::value<int>()->default_value(100), "client's rpc timeout(in milli seconds)")
      ("times", po::value<int>()->default_value(64), "client's rpc times")
      ("threadpool-size,t", po::value<int>()->default_value(16), "thread pool size");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
      std::cout << desc << std::endl;
      return 0;
    }

    int port = vm["port"].as<int>();
    int clients = vm["clients"].as<int>();
    int timeout = vm["timeout"].as<int>();
    int times = vm["times"].as<int>();
    int threadpool_size = vm["threadpool-size"].as<int>();


    boost::asio::io_service io_service;
    boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), port);

    std::vector<Session> session(clients);
    for (size_t i=0; i<session.size(); i++)
    {
      boost::shared_ptr<boost::asio::ip::tcp::socket>
        socket(new boost::asio::ip::tcp::socket(io_service));

      socket->connect(endpoint);

      session[i].client.reset(new AsyncEchoServerClient(socket));
      session[i].client->set_rpc_timeout(timeout);
      session[i].times = times;
      session[i].request.__isset.message = true;
      session[i].request.message = "Hello World";

      session[i].client->async_echo(session[i]._return, session[i].request,
        boost::bind(echo_callback, session[i].client.get(),
        &session[i]._return, &session[i].request, &session[i].times, _1));
    }

    boost::thread_group tg;
    for (int i=0; i<threadpool_size; i++)
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
