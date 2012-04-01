/** @file
* @brief
* @author yafei.zhang@langtaojin.com
* @date
* @version
*
*/
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include <io_service_pool.h>
#include "gen-cpp/AsyncEchoServer.h"

using namespace ::com::langtaojin::adgaga;
using namespace ::apache::thrift::async;

struct SessionClient
{
  boost::shared_ptr<AsyncEchoServerClient> rpc_channel;
  Response _return;
  Request request;
  bool pending;
  bool success;

  SessionClient(): pending(true), success(false) {}
};

struct Session
{
  boost::mutex mutex;

  std::vector<SessionClient> clients;

  boost::shared_ptr<boost::asio::io_service::strand> strand;
  boost::shared_ptr<boost::asio::deadline_timer> timer;

  size_t timeout;

  size_t result_count;
  size_t ok_result_count;

  Session(): timeout(0), result_count(0), ok_result_count(0)
  {
  }

  ~Session()
  {
  }
};

void on_timeout(boost::shared_ptr<Session> session,
                const boost::system::error_code& ec)
{
  if (ec != boost::asio::error::operation_aborted)
  {
    session->timer.reset();

    for (size_t i=0; i<session->clients.size(); i++)
    {
      SessionClient& client = session->clients[i];
      if (client.pending)
      {
        client.rpc_channel->close();
      }
    }
  }
}

void simulate_client(int clients,
                     const boost::asio::ip::tcp::endpoint& endpoint,
                     io_service_pool * pool);

void on_echo(boost::shared_ptr<Session> session,
             size_t client_slot,
             const boost::system::error_code& ec,
             int clients,
             const boost::asio::ip::tcp::endpoint& endpoint,
             io_service_pool * pool)
{
  SessionClient& client = session->clients[client_slot];
  client.pending = false;
  session->result_count++;

  if (!ec)
  {
    client.success = true;
    session->ok_result_count++;
  }
  else
  {
    client.success = false;
  }

  if (session->result_count == session->clients.size())
  {
    if (session->timer)
    {
      session->timer->cancel();
    }

    session.reset();

    simulate_client(clients-1, endpoint, pool);
  }
}

void stop_impl(io_service_pool * pool)
{
  printf("stopping\n");
  pool->stop();
  printf("stopped\n");
}

void stop(io_service_pool * pool)
{
  pool->get_io_service(0).post(boost::bind(stop_impl, pool));
}

void simulate_client(int clients,
                     const boost::asio::ip::tcp::endpoint& endpoint,
                     io_service_pool * pool)
{
  if (clients <= 0)
  {
    stop(pool);
    return;
  }

  boost::system::error_code ec;
  boost::shared_ptr<Session> session(new Session);
  boost::asio::io_service& _ios = pool->get_io_service();
  size_t clients_number = 2;

  session->clients.resize(clients_number);
  session->strand.reset(new boost::asio::io_service::strand(_ios));
  session->timer.reset(new boost::asio::deadline_timer(_ios));

  for (size_t j=0; j<clients_number; j++)
  {
    boost::shared_ptr<boost::asio::ip::tcp::socket> socket(
      new boost::asio::ip::tcp::socket(_ios));

    socket->connect(endpoint, ec);
    if (ec)
    {
      printf("connect: %s\n", ec.message().c_str());
      stop(pool);
      return;
    }

    SessionClient& client = session->clients[j];

    client.rpc_channel.reset(new AsyncEchoServerClient(socket));

    client.rpc_channel->set_strand(session->strand);
    client.request.__isset.message = true;
    client.request.message = "Hello World";
  }

  session->timer->expires_from_now(boost::posix_time::milliseconds(session->timeout));
  session->timer->async_wait(session->strand->wrap(
    boost::bind(on_timeout, session, _1)));

  for (size_t j=0; j<clients_number; j++)
  {
    SessionClient& client = session->clients[j];
    client.rpc_channel->async_echo(client._return, client.request,
      boost::bind(on_echo, session, j, _1, clients, endpoint, pool));
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
      ("clients,c", po::value<int>()->default_value(1), "clients' number")
      ("threadpool-size,t", po::value<int>()->default_value(1), "thread pool size");

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
    int threadpool_size = vm["threadpool-size"].as<int>();

    boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), port);

    io_service_pool pool(threadpool_size);

    simulate_client(clients, endpoint, &pool);

    boost::thread_group tg;
    tg.create_thread(boost::bind(&io_service_pool::run, &pool, true));
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
