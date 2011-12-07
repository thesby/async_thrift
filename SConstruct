import os
import os.path

env = Environment()
env = env.Clone()
env.Append(CCFLAGS = Split('-Wall -g -O2'))
env.Append(CPPPATH = Split('include example/gen-cpp /usr/local/include/thrift'))


env.StaticLibrary('async_thrift',
    [
        'lib/AsyncThriftClient.cpp',
        'lib/AsyncThriftServer.cpp',
        'lib/AsyncConnection.cpp',
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


# def generate_thrift(target, source, env):
   # for i in range(len(source)):
       # s = source[i]
       # cwd = os.path.dirname(str(s))
       # if len(cwd) == 0:
           # cwd = './'
       # if str(s).endswith('.thrift'):
           # os.system('./thrift_0.5.0_patch/thrift -o %s --gen cpp:pure_enums %s' % (cwd, s))

# output = [
       # 'example/gen-cpp/EchoServer.cpp',
       # 'example/gen-cpp/EchoServer.h',
       # 'example/gen-cpp/AsyncEchoServerClient.cpp',
       # 'example/gen-cpp/AsyncEchoServerClient.h',
       # 'example/gen-cpp/test_constants.cpp',
       # 'example/gen-cpp/test_constants.h',
       # 'example/gen-cpp/test_types.cpp',
       # 'example/gen-cpp/test_types.h',
       # 'example/gen-cpp/EchoServer_server.skeleton.cpp',
        # ]

# Command(output,
       # ['example/test.thrift'],
       # generate_thrift)

# Source = [
   # 'example/gen-cpp/EchoServer.cpp',
   # 'example/gen-cpp/AsyncEchoServerClient.cpp',
   # 'example/gen-cpp/test_constants.cpp',
   # 'example/gen-cpp/test_types.cpp',
   # 'lib/AsyncThriftClient.cpp',
   # 'lib/AsyncThriftServer.cpp',
   # 'lib/AsyncConnection.cpp',
# ]

# env.Program('server',
   # Source + ['example/EchoServer_server.cpp'],
# )

# env.Program('client',
   # Source + ['example/AsyncEchoServerClientTest.cpp'],
# )

