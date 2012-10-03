all:
	scons


clean:
	rm -f bin/*
	rm -f log/*
	find . -name '*.o' | xargs rm -f
	find . -name '*~' | xargs rm -f
	find . -name 'core.*' | xargs rm -f

cs:
	cscope -Rb
	ctags -R

indent:
	find src common -name "*.c" | xargs indent -npro -kr -i4 -ts4 -sob -l120 -ss -ncs -cp1 --no-tabs
	find src common -name "*.h" | xargs indent -npro -kr -i4 -ts4 -sob -l120 -ss -ncs -cp1 --no-tabs

ck:
	#use master only when ismaster msg, we use 'primary' otherwise
	cat src/*.[c,h] | grep 'master' | grep -v ismaster
