#env = Environment( CC = '/opt/local/bin/gcc', CXX = '/opt/local/bin/g++' )
env = Environment()

print "CC is:", env['CC']
print "CXX is:", env['CXX']

env.Append(CPPPATH = ['/opt/local/include/'])

env.Append(CPPFLAGS = ['-O3', '-std=c++1y', '-pthread'])

env.Append(LIBPATH = ['/opt/local/lib/'])

env.Append(LIBS = ['pthread', 'tbb', 'tbbmalloc'])

#env.Append(LINKFLAGS = ['-Wl,--no-as-needed'])

t = env.Program(target='main', source=['./pipeline.cpp', './main.cpp'])

Default(t)
