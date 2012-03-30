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
#include "gen-cpp/AsyncEchoServer.h"

using namespace com::langtaojin::adgaga;

struct Session
{
  boost::shared_ptr<AsyncEchoServerClient> client;
  boost::shared_ptr<boost::asio::io_service::strand> strand;
  boost::shared_ptr<boost::asio::deadline_timer> timer;

  size_t timeout;
  size_t times;
  Response _return;
  Request request;
};


void on_timeout(Session * session,
                const boost::system::error_code& ec)
{
  if (ec != boost::asio::error::operation_aborted)
  {
    printf("%s %p time out\n", __FUNCTION__, session);
    //session->client->cancel();
    session->client->close();
  }
}

void on_echo(Session * session,
             const boost::system::error_code& ec)
{
  if (!ec)
  {
    //if (--session->times != 0)
    //{
      //session->timer->expires_from_now(boost::posix_time::milliseconds(session->timeout));
    session->times++;
    if (session->client->is_open())
      session->client->async_echo(session->_return, session->request,
        boost::bind(on_echo, session, _1));
    //}
  }
  else
  {
    printf("%s %p %s. RPC %u times\n",
      __FUNCTION__, session, ec.message().c_str(), static_cast<uint32_t>(session->times));
  }
}

int main(int argc, char **argv)
{
  try
  {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help,h", "produce help message")
      ("port,p", po::value<int>()->default_value(12500), "listening port")
      ("clients,c", po::value<int>()->default_value(32), "clients' number")
      //("times", po::value<int>()->default_value(64), "client's rpc times")
      ("threadpool-size,t", po::value<int>()->default_value(32), "thread pool size");

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
    //int times = vm["times"].as<int>();
    int threadpool_size = vm["threadpool-size"].as<int>();


    boost::asio::io_service io_service;
    boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), port);
    boost::system::error_code ec;

    std::vector<Session> session(clients);

    for (size_t i=0; i<session.size(); i++)
    {
      boost::shared_ptr<boost::asio::ip::tcp::socket> socket(
        new boost::asio::ip::tcp::socket(io_service));

      socket->connect(endpoint, ec);
      if (ec)
      {
        printf("connect: %s\n", ec.message().c_str());
        continue;
      }

      Session& se = session[i];

      se.client.reset(new AsyncEchoServerClient(socket));
      se.strand.reset(new boost::asio::io_service::strand(io_service));
      se.timer.reset(new boost::asio::deadline_timer(io_service));
      se.timeout = 20;
      se.times = 0;
      se.request.__isset.message = true;
      se.request.message = "Hello World";

      se.timer->expires_from_now(boost::posix_time::milliseconds(se.timeout));
      se.timer->async_wait(se.strand->wrap(boost::bind(on_timeout, &se, _1)));

      se.client->set_strand(se.strand);
      se.client->async_echo(se._return, se.request,
        boost::bind(on_echo, &se, _1));
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
