/** @file
 * @brief ThreadPoolServer that can run against avalanche
 * When the there are too many pending tasks, close the listening port.
 * Server may re-listen the port until too many pending tasks have gone.
 *
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include "thread_pool_server.h"
#include <transport/TTransportException.h>
#include <concurrency/Exception.h>
#include <concurrency/Thread.h>
#include <concurrency/PosixThreadFactory.h>

namespace apache { namespace thrift { namespace sync {

  using ::apache::thrift::concurrency::PosixThreadFactory;
  using ::apache::thrift::concurrency::TooManyPendingTasksException;
  using ::apache::thrift::protocol::TProtocol;
  using ::apache::thrift::server::TServerEventHandler;
  using ::apache::thrift::transport::TTransport;
  using ::apache::thrift::transport::TTransportException;

  class ThreadPoolServer::Task : public Runnable
  {
    public:
      Task(ThreadPoolServer& server,
          boost::shared_ptr<TProcessor> processor,
          boost::shared_ptr<TProtocol> input,
          boost::shared_ptr<TProtocol> output)
        : server_(server),
        processor_(processor),
        input_(input),
        output_(output)
    {}

      ~Task() {}

      void run()
      {
        boost::shared_ptr<TServerEventHandler> eventHandler =
          server_.getEventHandler();

        if (eventHandler != NULL)
        {
          eventHandler->clientBegin(input_, output_);
        }

        try
        {
          while (!server_.stop_ && processor_->process(input_, output_))
          {
            if (!input_->getTransport()->peek())
            {
              break;
            }
          }
        }
        catch (TTransportException& e)
        {
          // This is reasonably expected, client didn't send a full request so just ignore him
        }
        catch (TException& e)
        {
          GlobalOutput.printf("ThreadPoolServer::Task::run() TException: %s", e.what());
        }
        catch (std::exception &e)
        {
          GlobalOutput.printf("ThreadPoolServer::Task::run() std::exception: %s", e.what());
        }
        catch (...)
        {
          GlobalOutput("ThreadPoolServer::Task::run() Unknown exception");
        }

        if (eventHandler != NULL)
        {
          eventHandler->clientEnd(input_, output_);
        }

        try
        {
          input_->getTransport()->close();
        }
        catch (TTransportException& e)
        {
          GlobalOutput.printf("ThreadPoolServer::Task::run() input close failed: %s", e.what());
        }
        try
        {
          output_->getTransport()->close();
        }
        catch (TTransportException& e)
        {
          GlobalOutput.printf("ThreadPoolServer::Task::run() output close failed: %s", e.what());
        }
      }

    private:
      ThreadPoolServer& server_;
      boost::shared_ptr<TProcessor> processor_;
      boost::shared_ptr<TProtocol> input_;
      boost::shared_ptr<TProtocol> output_;
  };


  ThreadPoolServer::ThreadPoolServer(
      boost::shared_ptr<TProcessor> processor,
      boost::shared_ptr<TServerTransport> serverTransport,
      boost::shared_ptr<TTransportFactory> transportFactory,
      boost::shared_ptr<TProtocolFactory> protocolFactory,
      int64_t addTaskTimeout,
      int64_t addTaskExpiration,
      size_t workerThreadCount,
      size_t pendingTaskCountMax)
    : BaseServer(processor, serverTransport, transportFactory, protocolFactory),
    addTaskTimeout_(addTaskTimeout),
    addTaskExpiration_(addTaskExpiration),
    workerThreadCount_(workerThreadCount),
    pendingTaskCountMax_(pendingTaskCountMax)
  {
    threadManager_ = ThreadManager::newSimpleThreadManager(workerThreadCount_, pendingTaskCountMax_);

    boost::shared_ptr<PosixThreadFactory> threadFactory =
      boost::shared_ptr<PosixThreadFactory>(new PosixThreadFactory());
    threadManager_->threadFactory(threadFactory);

    threadManager_->start();
  }


  ThreadPoolServer::~ThreadPoolServer()
  {}


  void ThreadPoolServer::serve()
  {
    boost::shared_ptr<TTransport> client;
    boost::shared_ptr<TTransport> inputTransport;
    boost::shared_ptr<TTransport> outputTransport;
    boost::shared_ptr<TProtocol> inputProtocol;
    boost::shared_ptr<TProtocol> outputProtocol;
    size_t pendingTaskCount;
    size_t pendingTaskCountMax;


    if (!startListen())
      return;

    // Run the preServe event
    if (eventHandler_ != NULL)
    {
      eventHandler_->preServe();
    }

    stop_ = false;

    while (!stop_)
    {

      pendingTaskCount = threadManager_->pendingTaskCount();
      pendingTaskCountMax = threadManager_->pendingTaskCountMax();
      if (pendingTaskCount >= pendingTaskCountMax)
      {
        if (isListening())
        {
          safeStopListen();

          GlobalOutput.printf("ThreadPoolServer::serve() Too many pending tasks: %u %u",
              static_cast<uint32_t>(pendingTaskCount), static_cast<uint32_t>(pendingTaskCountMax));
        }

        yield();
        continue;
      }
      else
      {
        if (!isListening())
        {
          GlobalOutput.printf("ThreadPoolServer::serve() Recover from Too many pending tasks: %u %u",
              static_cast<uint32_t>(pendingTaskCount), static_cast<uint32_t>(pendingTaskCountMax));

          if (!startListen())
            return;
        }
      }


      try
      {
        client.reset();
        inputTransport.reset();
        outputTransport.reset();
        inputProtocol.reset();
        outputProtocol.reset();

        // Fetch client from server
        client = serverTransport_->accept();

        // Make IO transports
        inputTransport = inputTransportFactory_->getTransport(client);
        outputTransport = outputTransportFactory_->getTransport(client);
        inputProtocol = inputProtocolFactory_->getProtocol(inputTransport);
        outputProtocol = outputProtocolFactory_->getProtocol(outputTransport);

        // Add to thread manager pool
        threadManager_->add(boost::shared_ptr<ThreadPoolServer::Task>(
              new ThreadPoolServer::Task(*this, processor_, inputProtocol, outputProtocol)),
            addTaskTimeout_, addTaskExpiration_);

      }
      catch (TooManyPendingTasksException& e)
      {
        if (inputTransport != NULL)
        {
          inputTransport->close();
        }
        if (outputTransport != NULL)
        {
          outputTransport->close();
        }
        if (client != NULL)
        {
          client->close();
        }

        safeStopListen();

        GlobalOutput.printf("ThreadPoolServer::serve() Too many pending tasks: %u %u: %s",
            static_cast<uint32_t>(pendingTaskCount), static_cast<uint32_t>(pendingTaskCountMax),
            e.what());
        continue;
      }
      catch (TTransportException& e)
      {
        if (inputTransport != NULL)
        {
          inputTransport->close();
        }
        if (outputTransport != NULL)
        {
          outputTransport->close();
        }
        if (client != NULL)
        {
          client->close();
        }
        if (!stop_ || e.getType() != TTransportException::INTERRUPTED)
        {
          GlobalOutput.printf("ThreadPoolServer::serve() TServerTransport died on accept: %s", e.what());
        }
        continue;
      }
      catch (TException& e)
      {
        if (inputTransport != NULL)
        {
          inputTransport->close();
        }
        if (outputTransport != NULL)
        {
          outputTransport->close();
        }
        if (client != NULL)
        {
          client->close();
        }
        GlobalOutput.printf("ThreadPoolServer::serve() TException: %s", e.what());
        continue;
      }
      catch (...)
      {
        if (inputTransport != NULL)
        {
          inputTransport->close();
        }
        if (outputTransport != NULL)
        {
          outputTransport->close();
        }
        if (client != NULL)
        {
          client->close();
        }
        GlobalOutput("ThreadPoolServer::serve() Unknown exception");
        break;
      }
    }


    // If stopped manually, join the existing threads
    if (stop_)
    {
      try
      {
        stopListen();
        threadManager_->join();
      }
      catch (TException& e)
      {
        GlobalOutput.printf("ThreadPoolServer::serve() TException shutting down: %s", e.what());
      }
      stop_ = false;
    }
  }

} } }
