env = Environment()

env.Append(CPPPATH = ['/usr/include/', '/opt/intel/tbb/include/'])

env.Append(CCFLAGS = ['-O3', '-std=c++11', '-pthread'])

env.Append(LIBPATH = ['/usr/lib/', '/opt/intel/tbb/lib/'])

env.Append(LIBS = [ 'pthread', 'tbb' ])

#env.Append(LINKFLAGS = ['-Wl,--no-as-needed'])

t = env.Program(target='main', source=['./common.cpp', './main.cpp'])

Default(t)
