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

env.Append(CCFLAGS = Split('-Wall -g -O2'))
env.Append(CPPPATH = Split('async sync fb303 /usr/local/include/thrift'))
env.Append(LIBS = [
    File('./libthrift_ext.a'),
    FindStaticLib('boost_thread'),
    FindStaticLib('boost_system'),
    FindStaticLib('boost_program_options'),
    FindStaticLib('gflags'),
    FindStaticLib('thrift'),
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
            os.system('%s/thrift_0.5.0_patch/thrift -o %s --gen py %s' % (scons_cwd, cwd, s))

output = [
    'fb303/gen-cpp/AsyncFacebookService.cpp',
    'fb303/gen-cpp/AsyncFacebookService.h',
    'fb303/gen-cpp/FacebookService.cpp',
    'fb303/gen-cpp/FacebookService.h',
    'fb303/gen-cpp/fb303_constants.cpp',
    'fb303/gen-cpp/fb303_constants.h',
    'fb303/gen-cpp/fb303_types.cpp',
    'fb303/gen-cpp/fb303_types.h',

    'fb303/gen-cpp/AsyncService.cpp',
    'fb303/gen-cpp/AsyncService.h',
    'fb303/gen-cpp/Service.cpp',
    'fb303/gen-cpp/Service.h',
    'fb303/gen-cpp/thrift_ext_constants.cpp',
    'fb303/gen-cpp/thrift_ext_constants.h',
    'fb303/gen-cpp/thrift_ext_types.cpp',
    'fb303/gen-cpp/thrift_ext_types.h',

    'test/gen-cpp/EchoServer.cpp',
    'test/gen-cpp/EchoServer.h',
    'test/gen-cpp/AsyncEchoServer.cpp',
    'test/gen-cpp/AsyncEchoServer.h',
    'test/gen-cpp/test_constants.cpp',
    'test/gen-cpp/test_constants.h',
    'test/gen-cpp/test_types.cpp',
    'test/gen-cpp/test_types.h',

    'test/gen-cpp/AsyncFacebookService.cpp',
    'test/gen-cpp/AsyncFacebookService.h',
    'test/gen-cpp/FacebookService.cpp',
    'test/gen-cpp/FacebookService.h',
    'test/gen-cpp/fb303_constants.cpp',
    'test/gen-cpp/fb303_constants.h',
    'test/gen-cpp/fb303_types.cpp',
    'test/gen-cpp/fb303_types.h',

    'test/gen-cpp/AsyncService.cpp',
    'test/gen-cpp/AsyncService.h',
    'test/gen-cpp/Service.cpp',
    'test/gen-cpp/Service.h',
    'test/gen-cpp/thrift_ext_constants.cpp',
    'test/gen-cpp/thrift_ext_constants.h',
    'test/gen-cpp/thrift_ext_types.cpp',
    'test/gen-cpp/thrift_ext_types.h',
]

Command(output,
    ['fb303/fb303.thrift', 'fb303/thrift_ext.thrift', 'test/test.thrift', 'test/fb303.thrift', 'test/thrift_ext.thrift'],
    generate_thrift
)

env.StaticLibrary('thrift_ext',
    [
        'fb303/gen-cpp/AsyncFacebookService.cpp',
        'fb303/gen-cpp/FacebookService.cpp',
        'fb303/gen-cpp/fb303_constants.cpp',
        'fb303/gen-cpp/fb303_types.cpp',

        'fb303/gen-cpp/AsyncService.cpp',
        'fb303/gen-cpp/Service.cpp',
        'fb303/gen-cpp/thrift_ext_constants.cpp',
        'fb303/gen-cpp/thrift_ext_types.cpp',

        'fb303/async_facebook_base.cpp',
        'fb303/process_stat.c',
        'fb303/service_base_handler.cpp',

        'async/asio_pool.cpp',
        'async/async_client.cpp',
        'async/async_exception.cpp',
        'async/async_processor.cpp',
        'async/async_server.cpp',
        'async/async_util.cpp',
        'async/io_service_pool.cpp',
        'async/service_manager.cpp',

        'sync/base_server.cpp',
        'sync/host.cpp',
        'sync/threaded_server.cpp',
        'sync/thread_pool_server.cpp',
    ]
)

TestSource = [
    'test/gen-cpp/EchoServer.cpp',
    'test/gen-cpp/AsyncEchoServer.cpp',
    'test/gen-cpp/test_constants.cpp',
    'test/gen-cpp/test_types.cpp',
#    'test/gen-cpp/AsyncFacebookService.cpp',
#    'test/gen-cpp/FacebookService.cpp',
#    'test/gen-cpp/fb303_constants.cpp',
#    'test/gen-cpp/fb303_types.cpp',
#    'test/gen-cpp/AsyncService.cpp',
#    'test/gen-cpp/Service.cpp',
#    'test/gen-cpp/thrift_ext_constants.cpp',
#    'test/gen-cpp/thrift_ext_types.cpp',
]

env.Program('fb303_shutdown',
    ['fb303/fb303_shutdown.cpp'],
)

env.Program('echo_server',
    TestSource + ['test/echo_server.cpp'],
)

env.Program('echo_server_perf_test',
    TestSource + ['test/echo_server_perf_test.cpp'],
)

env.Program('echo_server_test',
    TestSource + ['test/echo_server_test.cpp'],
)

env.Program('asio_pool_test',
    ['test/asio_pool_test.cpp'],
)

env.Program('service_manager_test',
    ['test/service_manager_test.cpp'],
)

env.Program('io_service_pool_test',
    ['test/io_service_pool_test.cpp'],
)

env.Program('to_string_test',
    TestSource + ['test/to_string_test.cpp'],
)

env.Program('thrift_pool_test',
    ['test/thrift_pool_test.cpp'],
)
