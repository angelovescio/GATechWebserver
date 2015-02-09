CFLAGS := -w --std=gnu99 -ggdb

ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread
endif

#default: webserver webclient
SOURCECLIENT=optlist/optlist.c clientserver.c stats.c webclient.c
SOURCESERVER=optlist/optlist.c clientserver.c stats.c webserver.c 
OBJECTCLIENT=$(SOURCECLIENT:.c=.o)
OBJECTSERVER=$(SOURCESERVER:.c=.o)

all: $(SOURCESERVER) $(SOURCESERVER) webserver webclient

.cpp.o:
	echo "Compiling C++"
	$(CC) $(CFLAGS) -c $< -o $@
.c.o:
	echo "Compiling C"
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -fr optlist\*.o *.o webserver webclient

webserver: $(OBJECTSERVER) 
	$(CC) $(OBJECTSERVER) -lpthread -o $@
webclient: $(OBJECTCLIENT) 
	$(CC) $(OBJECTCLIENT) -lpthread -o $@