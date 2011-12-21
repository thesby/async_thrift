import os
import os.path

env = Environment()
env = env.Clone()
env.Append(CCFLAGS = Split('-Wall -g -O2'))
env.Append(CPPPATH = Split('src fb303 /usr/local/include/thrift'))

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
        'fb303/gen-cpp/AsyncFacebookService.cpp',
        'fb303/gen-cpp/AsyncFacebookService.h',
        'fb303/gen-cpp/FacebookService.cpp',
        'fb303/gen-cpp/FacebookService.h',
        'fb303/gen-cpp/fb303_constants.cpp',
        'fb303/gen-cpp/fb303_constants.h',
        'fb303/gen-cpp/fb303_types.cpp',
        'fb303/gen-cpp/fb303_types.h',

        'example/gen-cpp/BaseServer.cpp',
        'example/gen-cpp/BaseServer.h',
        'example/gen-cpp/AsyncEchoServer.cpp',
        'example/gen-cpp/AsyncEchoServer.h',
        'example/gen-cpp/base_constants.cpp',
        'example/gen-cpp/base_constants.h',
        'example/gen-cpp/base_types.cpp',
        'example/gen-cpp/base_types.h',

        'example/gen-cpp/EchoServer.cpp',
        'example/gen-cpp/EchoServer.h',
        'example/gen-cpp/AsyncBaseServer.cpp',
        'example/gen-cpp/AsyncBaseServer.h',
        'example/gen-cpp/test_constants.cpp',
        'example/gen-cpp/test_constants.h',
        'example/gen-cpp/test_types.cpp',
        'example/gen-cpp/test_types.h',
        ]

Command(output,
       ['fb303/fb303.thrift', 'example/base.thrift', 'example/test.thrift'],
       generate_thrift)

env.StaticLibrary('async_thrift',
    [
        'src/AsyncConnection.cpp',
        'src/AsyncProcessor.cpp',
        'src/AsyncThriftClient.cpp',
        'src/AsyncThriftServerBase.cpp',
        'src/AsyncThriftServer.cpp',
        'src/AsyncThriftServerEx.cpp',
        'src/io_service_pool.cpp',
    ]
)

env.StaticLibrary('async_fb303',
    [
        'fb303/gen-cpp/AsyncFacebookService.cpp',
        'fb303/gen-cpp/FacebookService.cpp',
        'fb303/gen-cpp/fb303_constants.cpp',
        'fb303/gen-cpp/fb303_types.cpp',
        'fb303/AsyncFacebookBase.cpp',
    ]
)

Source = [
   'example/gen-cpp/BaseServer.cpp',
   'example/gen-cpp/AsyncBaseServer.cpp',
   'example/gen-cpp/base_constants.cpp',
   'example/gen-cpp/base_types.cpp',

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

env.Program('perf_test',
   Source + ['example/EchoServerPerfTest.cpp'],
)

