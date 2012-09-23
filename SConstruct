#coding: utf-8
import glob, os, subprocess, sys

src = glob.glob('src/*.c')

LIBS = ['event', 'gcov']
LIBPATH = ['/usr/local/lib', '/usr/lib'] #顺序很重要

CPPPATH = []

CCFLAGS='' # -pg is for gprof  osd 不能用 -D_FILE_OFFSET_BITS=64
LINKFLAGS=' '

#CCFLAGS='-D_DEBUG -Wall -g -Wno-pointer-sign -pg -fprofile-arcs -ftest-coverage' # -pg is for gprof
#LINKFLAGS=' -pg '

Program( 'test', src, LIBS = LIBS, LIBPATH = LIBPATH, CPPPATH = CPPPATH, CCFLAGS = CCFLAGS)

