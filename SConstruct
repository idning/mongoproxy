#coding: utf-8
import glob, os, subprocess, sys

src = glob.glob('src/*.c') + glob.glob('common/*.c') + glob.glob('lib/*.c')

#LIBS = ['event', 'gcov', ""]
#LIBS = ['event', 'tcmalloc']
LIBS = ['event']
LIBPATH = ['/usr/local/lib'] 


CPPPATH = ['usr/local/include', 'common', 'lib']

CCFLAGS='-Wall -g ' 
LINKFLAGS='-g '

if os.sys.platform in ["darwin", "linux2"]:
    CCFLAGS += " -DMONGO_HAVE_STDINT " 


Program( 'bin/mongoproxy', src, LIBS = LIBS, LIBPATH = LIBPATH, CPPPATH = CPPPATH, CCFLAGS = CCFLAGS)
Program( 'test/test', 'test/test.c', LIBS = LIBS, LIBPATH = LIBPATH, CPPPATH = CPPPATH, CCFLAGS = CCFLAGS)

