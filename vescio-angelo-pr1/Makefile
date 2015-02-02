CFLAGS := -Wall --std=gnu99 -ggdb

ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread
endif

#default: webserver webclient
SOURCECLIENT=webclient.c
SOURCESERVER=optlist/optlist.c webserver.c 
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
	$(CC) $(OBJECTSERVER) -o $@
webclient: $(OBJECTCLIENT) 
	$(CC) $(OBJECTCLIENT) -o $@