/** @file
 * @brief ThreadedServer that can run against avalanche
 * When the there are too many pending tasks, close the listening port.
 * Server may re-listen the port until too many pending tasks have gone.
 *
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#include "threaded_server.h"
#include <concurrency/PosixThreadFactory.h>

//lint -esym(1712,Task) default constructor not defined
//lint -e58 lint bug

namespace apache { namespace thrift { namespace sync {

  using ::apache::thrift::concurrency::PosixThreadFactory;
  using ::apache::thrift::concurrency::Synchronized;
  using ::apache::thrift::concurrency::Thread;
  using ::apache::thrift::protocol::TProtocol;
  using ::apache::thrift::server::TServerEventHandler;
  using ::apache::thrift::transport::TTransport;
  using ::apache::thrift::transport::TTransportException;

  class ThreadedServer::Task: public Runnable
  {
    public:
      Task(ThreadedServer& _server,
          boost::shared_ptr<TProcessor> processor,
          boost::shared_ptr<TProtocol> input,
          boost::shared_ptr<TProtocol> output)
        : server_(_server),
        processor_(processor),
        input_(input),
        output_(output)
    {}

      ~Task() {}

      void run()
      {
        boost::shared_ptr<TServerEventHandler> eventHandler = server_.getEventHandler();

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
        catch (TTransportException& /*e*/)
        {
          // This is reasonably expected, client didn't send a full request so just ignore him
        }
        catch (TException& e)
        {
          GlobalOutput.printf("ThreadedServer::Task::run() TException: %s", e.what());
        }
        catch (std::exception& e)
        {
          GlobalOutput.printf("ThreadedServer::Task::run() std::exception: %s", e.what());
        }
        catch (...)
        {
          GlobalOutput("ThreadedServer::Task::run() Unknown exception");
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
          GlobalOutput.printf("ThreadedServer::Task::run() input close failed: %s", e.what());
        }
        try
        {
          output_->getTransport()->close();
        }
        catch (TTransportException& e)
        {
          GlobalOutput.printf("ThreadedServer::Task::run() output close failed: %s", e.what());
        }

        // Remove this task from parent bookkeeping
        //lint -esym(1788,s) Variable 's'is referenced only by its constructor or destructor
        {
          Synchronized s(server_.tasksMonitor_);
          (void)server_.tasks_.erase(this);
          if (server_.tasks_.empty())
          {
            server_.tasksMonitor_.notify();
          }
        }
      }

    private:
      ThreadedServer& server_;
      boost::shared_ptr<TProcessor> processor_;
      boost::shared_ptr<TProtocol> input_;
      boost::shared_ptr<TProtocol> output_;
  };


  ThreadedServer::ThreadedServer(
      boost::shared_ptr<TProcessor> processor,
      boost::shared_ptr<TServerTransport> serverTransport,
      boost::shared_ptr<TTransportFactory> transportFactory,
      boost::shared_ptr<TProtocolFactory> protocolFactory,
      size_t workerThreadCountMax)
    : BaseServer(processor, serverTransport, transportFactory, protocolFactory),
    workerThreadCountMax_(workerThreadCountMax)
  {
    threadFactory_ = boost::shared_ptr<PosixThreadFactory>(new PosixThreadFactory());
  }


  ThreadedServer::~ThreadedServer() {}


  void ThreadedServer::serve()
  {
    boost::shared_ptr<TTransport> client;
    boost::shared_ptr<TTransport> inputTransport;
    boost::shared_ptr<TTransport> outputTransport;
    boost::shared_ptr<TProtocol> inputProtocol;
    boost::shared_ptr<TProtocol> outputProtocol;
    size_t workerThreadCount;

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
      {
        Synchronized s(tasksMonitor_);
        workerThreadCount = tasks_.size();
      }

      if (workerThreadCount >= workerThreadCountMax_)
      {
        if (isListening())
        {
          safeStopListen();

          GlobalOutput.printf("ThreadedServer::serve() Too many threads: %u %u",
              static_cast<uint32_t>(workerThreadCount), static_cast<uint32_t>(workerThreadCountMax_));
        }

        yield();
        continue;
      }
      else
      {
        if (!isListening())
        {
          GlobalOutput.printf("ThreadedServer::serve() Recover from Too many threads: %u %u",
              static_cast<uint32_t>(workerThreadCount), static_cast<uint32_t>(workerThreadCountMax_));

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

        // Create a task
        ThreadedServer::Task* task = new ThreadedServer::Task(*this,
            processor_,
            inputProtocol,
            outputProtocol);
        boost::shared_ptr<Runnable> runnable(task);

        // Create a thread for this task
        boost::shared_ptr<Thread> _thread = boost::shared_ptr<Thread>(threadFactory_->newThread(runnable));

        // Insert thread into the set of threads
        {
          Synchronized s(tasksMonitor_);
          (void)tasks_.insert(task);
        }

        // Start the thread!
        _thread->start();

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
          GlobalOutput.printf("ThreadedServer::serve() TServerTransport died on accept: %s", e.what());
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
        GlobalOutput.printf("ThreadedServer::serve() TException: %s", e.what());
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
        GlobalOutput("ThreadedServer::serve() Unknown exception");
        break;
      }
    }

    // If stopped manually, make sure to close server transport
    if (stop_)
    {
      try
      {
        //lint --e(118) lint bug
        //lint --e(1013) lint bug
        serverTransport_->close();
      }
      catch (TException& e)
      {
        GlobalOutput.printf("ThreadedServer::serve() TException shutting down: %s", e.what());
      }

      try
      {
        Synchronized s(tasksMonitor_);
        while (!tasks_.empty())
        {
          tasksMonitor_.wait();
        }
      }
      catch (TException& e)
      {
        GlobalOutput.printf("ThreadedServer::serve() TException joining workers: %s", e.what());
      }
      stop_ = false;
    }
  }

} } }
