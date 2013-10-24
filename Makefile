imp: imp.c object.c Makefile object.h
	clang -std=gnu99 -g -o imp imp.c object.c -L../../opt/libjit/lib64 -I../../opt/libjit/include -ljit
