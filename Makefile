all:
	scons


clean:
	rm mongoproxy 
	find . -name *.o | xargs rm 


cs:
	cscope -Rb
	ctags -R
