#coding: utf-8
import glob, os, subprocess, sys

src = glob.glob('src/*.c') + glob.glob('common/*.c') + glob.glob('lib/*.c')

#LIBS = ['event', 'gcov', ""]
#LIBS = ['event', 'tcmalloc']
LIBS = ['event']
LIBPATH = ['/usr/lib', '/usr/local/lib'] #顺序很重要


CPPPATH = ['common', 'lib']

CCFLAGS='-Wall -g ' # -pg is for gprof  osd 不能用 -D_FILE_OFFSET_BITS=64
LINKFLAGS='-g '

if os.sys.platform in ["darwin", "linux2"]:
    CCFLAGS += " -DMONGO_HAVE_STDINT " 


#CCFLAGS='-D_DEBUG -Wall -g -Wno-pointer-sign -pg -fprofile-arcs -ftest-coverage' # -pg is for gprof
#LINKFLAGS=' -pg '

Program( 'bin/mongoproxy', src, LIBS = LIBS, LIBPATH = LIBPATH, CPPPATH = CPPPATH, CCFLAGS = CCFLAGS)

Program( 'test/test', 'test/test.c', LIBS = LIBS, LIBPATH = LIBPATH, CPPPATH = CPPPATH, CCFLAGS = CCFLAGS)

