import os
import os.path

env = Environment()
env = env.Clone()
env.Append(CCFLAGS = Split('-Wall -g'))
env.Append(CPPPATH = Split('src example/gen-cpp /usr/local/include/thrift'))


env.StaticLibrary('async_thrift',
    [
        'src/AsyncThriftClient.cpp',
        'src/AsyncThriftServerBase.cpp',
        'src/AsyncThriftServer.cpp',
        'src/AsyncThriftServerEx.cpp',
        'src/AsyncConnection.cpp',
    ]
)

env.Append(LIBS = [
   File('/usr/local/lib/libthrift.a'),
   File('/usr/lib/libboost_thread.a'),
   File('/usr/lib/libboost_system.a'),
   File('/usr/lib/libboost_program_options.a'),
   'pthread',
   'rt',
])

scons_cwd = os.getcwd()

def generate_thrift(target, source, env):
   for i in range(len(source)):
       s = source[i]
       cwd = os.path.dirname(str(s))
       if len(cwd) == 0:
           cwd = './'
       if str(s).endswith('.thrift'):
           os.system('%s/thrift_0.5.0_patch/thrift -o %s --gen cpp:pure_enums %s' % (scons_cwd, cwd, s))

output = [
       'example/gen-cpp/EchoServer.cpp',
       'example/gen-cpp/EchoServer.h',
       'example/gen-cpp/AsyncEchoServer.cpp',
       'example/gen-cpp/AsyncEchoServer.h',
       'example/gen-cpp/test_constants.cpp',
       'example/gen-cpp/test_constants.h',
       'example/gen-cpp/test_types.cpp',
       'example/gen-cpp/test_types.h',
       'example/gen-cpp/EchoServer_server.skeleton.cpp',
        ]

Command(output,
       ['example/test.thrift'],
       generate_thrift)

Source = [
   'example/gen-cpp/EchoServer.cpp',
   'example/gen-cpp/AsyncEchoServer.cpp',
   'example/gen-cpp/test_constants.cpp',
   'example/gen-cpp/test_types.cpp',
]

COMMON_LIBS = [
    File('./libasync_thrift.a'),
    File('/usr/lib/libboost_thread.a'),
    File('/usr/lib/libboost_system.a'),
    File('/usr/lib/libboost_program_options.a'),
    File('/usr/local/lib/libthrift.a'),
    'pthread',
    'rt',
]

env.Append(LIBS = COMMON_LIBS)

env.Program('server',
   Source + ['example/EchoServer_server.cpp'],
)

env.Program('client',
   Source + ['example/AsyncEchoServerClientTest.cpp'],
)

