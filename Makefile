SHELL=C:/Windows/System32/cmd.exe
volobjects = main.o
animobjects = animEx.o half.o
LIBPATH = -L./lib
LIB = -lFreeImage
HEADERPATH = -I./include
STATICGCC = -static-libgcc -static-libstdc++

all : volEx.exe animEx.exe

volEx.exe : $(volobjects)
	g++ -Wall -O2 -g -ggdb -o $@ $(volobjects) $(LIBPATH) $(LIB) $(STATICGCC) $(HEADERPATH)

animEx.exe : $(animobjects)
	g++ -Wall -O2 -g -ggdb -o $@ $(animobjects) $(LIBPATH) $(LIB) $(STATICGCC) $(HEADERPATH)

%.o: %.cpp
	g++ -O2 -c -MMD -g -ggdb -o $@ $< $(HEADERPATH)

-include $(volobjects:.o=.d)
-include $(animobjects:.o=.d)

.PHONY : clean
clean :
	rm -rf volEx.exe animEx.exe *.o *.d
