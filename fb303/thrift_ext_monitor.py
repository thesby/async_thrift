#!/usr/bin/env python
# encoding=utf8

import os
import string
import gflags
from thrift.transport import TTransport
from thrift.transport import TSocket
from thrift.transport import THttpClient
from thrift.protocol import TBinaryProtocol
from thrift_ext.ttypes import *
import thrift_ext.Service


FLAGS = gflags.FLAGS
gflags.DEFINE_string('host', 'localhost:12500,localhost:12500', 'a host list to monitor(host:port,[host:port]...)') 


def thrift_ext_connect(host, port):
  try:
    socket = TSocket.TSocket(host, port)
    transport = TTransport.TFramedTransport(socket)
    protocol = TBinaryProtocol.TBinaryProtocol(transport)
    client = thrift_ext.Service.Client(protocol)
    transport.open()
    print '%s:%d has been connected' % (host, port)
    return client
  except Exception , e:
    print '%s:%d is not connected: %s' % (host, port, e)
    return None


def thrift_ext_close(client):
  if client is not None:
    client._iprot.trans.close()


def thrift_ext_Service_get_status_all(client):
  try:
    return client.get_status(), client.get_status_rt()
  except Exception , e:
    print 'RPC error: %s' % (e)
    return None, None


if __name__ == '__main__':
  try:
    sys.argv = FLAGS(sys.argv)  # parse flags
  except gflags.FlagsError, e:
    print '%s\nUsage: %s ARGS\n%s' % (e, sys.argv[0], FLAGS)
    sys.exit(1)

  for host_port in FLAGS.host.split(','):
    tmp = host_port.split(':')
    if len(tmp) != 2:
      continue

    host = tmp[0]
    port = string.atoi(tmp[1], 10)

    client = thrift_ext_connect(host, port)
    if client is not None:
      status, status_rt = thrift_ext_Service_get_status_all(client)
      print status
      print status_rt
      thrift_ext_close(client);

