PP:=g++
SOURCES:=evaluate.cpp reversi.cpp search.cpp zobrist.cpp
OBJECTS:=$(SOURCES:.cpp=.o)
DEPENDS:=$(SOURCES:.cpp=.d)

override CFLAGS+=-W
override CFLAGS+=-O2
override CFLAGS+=-std=c++11
override CFLAGS+=-DNDEBUG

learn: learn.o $(OBJECTS)
	$(PP) -o learn $(CFLAGS) $^ $(LIBS)

.cpp.o:
	$(PP) $(CFLAGS) -o $@ -c $<

%.d: %.cpp
	@$(SHELL) -c '$(CC) -MM $(CFLAGS) $< | sed "s|^.*:|$*.o $@:|g" > $@; [ -s $@ ] || rm -f $@'

clean:
	$(RM) learn $(OBJECTS) $(DEPENDS)

-include $(DEPENDS)
