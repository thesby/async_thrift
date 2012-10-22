#!/usr/bin/env python
# encoding=utf8

import pymongo
import gflags
import time


FLAGS = gflags.FLAGS
gflags.DEFINE_string('mongo_host',        'sdl-bi1,sdl-bi2',  'mongo host list')
gflags.DEFINE_string('mongo_db',          'db_rt',            'mongo db name')
gflags.DEFINE_string('mongo_collection',  'rtb_server',       'mongo collection name')


def mongo_inserter(record):
  record["t"] = long(time.time())
  connection = pymongo.Connection(FLAGS.mongo_host)
  db = pymongo.database.Database(connection, FLAGS.mongo_db)
  collection = pymongo.collection.Collection(db, FLAGS.mongo_collection)
  collection.insert(record)
