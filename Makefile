all:
	scons


clean:
	rm mongoproxy 
	find . -name *.o | xargs rm 


cs:
	cscope -Rb
	ctags -R

indent:
	find . -name "*.c" | xargs indent -npro -kr -i4 -ts4 -sob -l120 -ss -ncs -cp1 --no-tabs
	find . -name "*.h" | xargs indent -npro -kr -i4 -ts4 -sob -l120 -ss -ncs -cp1 --no-tabs

