import sys
import os

def FindStaticLib(libname, LIBRARY_PATH=os.environ.get('LIBRARY_PATH', '')):
    path_list=['.']
    if LIBRARY_PATH:
        path_list += LIBRARY_PATH.split('::')
    path_list += ['/lib', '/usr/lib', '/usr/local/lib']
    for path in path_list:
        fn = path + '/' + 'lib' + libname + '.a';
        if os.path.isfile(fn):
            return File(fn)
    return libname

env = Environment()
env = env.Clone()

if not env.GetOption('clean'):
    conf = Configure(env)
    conf.CheckCC()
    conf.CheckCXX()
    if not conf.CheckLibWithHeader('boost_thread', 'boost/thread.hpp', 'C++', autoadd=0):
        print 'Error: no boost'
        sys.exit()
    if not conf.CheckLibWithHeader('thrift', 'thrift/Thrift.h', 'C++', autoadd=0):
        print 'Error: no thrift'
        sys.exit()
    env = conf.Finish()

env.Append(CCFLAGS = Split('-Wall -g'))
env.Append(CPPPATH = Split('src fb303 /usr/local/include/thrift'))
env.Append(LIBS = [
    FindStaticLib('thrift'),
    FindStaticLib('boost_thread'),
    FindStaticLib('boost_system'),
    FindStaticLib('boost_program_options'),
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
        'src/asio_pool.cpp',
        'src/async_client.cpp',
        'src/async_exception.cpp',
        'src/async_processor.cpp',
        'src/async_server.cpp',
        'src/io_service_pool.cpp',
        'src/service_manager.cpp',
        'src/util.cpp',
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

env.Append(LIBS = [
    File('./libasync_thrift.a'),
    File('./libasync_fb303.a'),
    File('/usr/lib/libboost_thread.a'),
    File('/usr/lib/libboost_system.a'),
    File('/usr/lib/libboost_program_options.a'),
    File('/usr/local/lib/libthrift.a'),
    'pthread',
    'rt',
])

env.Program('echo_server',
    Source + ['example/echo_server.cpp'],
)

env.Program('echo_server_perf_test',
    Source + ['example/echo_server_perf_test.cpp'],
)

env.Program('echo_server_test',
    Source + ['example/echo_server_test.cpp'],
)

env.Program('asio_pool_test',
    Source + ['test/asio_pool_test.cpp'],
)

env.Program('service_manager_test',
    Source + ['test/service_manager_test.cpp'],
)

env.Program('io_service_pool_test',
    Source + ['test/io_service_pool_test.cpp'],
)

