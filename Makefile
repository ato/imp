imp: imp.c Makefile
	clang -std=gnu99 -g -o imp imp.c -L../../opt/libjit/lib64 -I../../opt/libjit/include -ljit
