all: xwebview

CC=gcc

LIBS=-lX11 -lXrandr -lXdamage -lXfixes -llz4
CFLAGS=-O3 -pipe -s -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
DEBUGCFLAGS=-Og -pipe -g -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -DIS_DEBUG

INPUT=xwebview.c wsServer/libws.a
OUTPUT=xwebview

RM=/bin/rm

.PHONY: xwebview
xwebview:
	$(MAKE) -C wsServer
	$(CC) $(INPUT) -o $(OUTPUT) $(LIBS) $(CFLAGS)

debug:
	$(CC) $(INPUT) -o $(OUTPUT) $(LIBS) $(DEBUGCFLAGS)

clean:
	if [ -e $(OUTPUT) ]; then $(RM) $(OUTPUT); fi
