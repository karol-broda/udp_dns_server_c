CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -Ilib/cJSON -Ilib/uthash

SRCDIR = src
INCDIR = include
LIBDIR = lib
OBJDIR = build

SRCS = $(SRCDIR)/dns_parser.c $(SRCDIR)/dns_server.c $(SRCDIR)/main.c $(LIBDIR)/cJSON/cJSON.c
OBJS = $(OBJDIR)/dns_parser.o $(OBJDIR)/dns_server.o $(OBJDIR)/main.o $(OBJDIR)/cJSON.o

TARGET = dns_server

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(OBJDIR)/dns_parser.o: $(SRCDIR)/dns_parser.c $(INCDIR)/dns_parser.h $(INCDIR)/dns_server.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $(SRCDIR)/dns_parser.c -o $(OBJDIR)/dns_parser.o

$(OBJDIR)/dns_server.o: $(SRCDIR)/dns_server.c $(INCDIR)/dns_records.h $(INCDIR)/dns_server.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $(SRCDIR)/dns_server.c -o $(OBJDIR)/dns_server.o

$(OBJDIR)/main.o: $(SRCDIR)/main.c $(INCDIR)/dns_parser.h $(INCDIR)/dns_records.h $(INCDIR)/dns_server.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $(SRCDIR)/main.c -o $(OBJDIR)/main.o

$(OBJDIR)/cJSON.o: $(LIBDIR)/cJSON/cJSON.c $(LIBDIR)/cJSON/cJSON.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $(LIBDIR)/cJSON/cJSON.c -o $(OBJDIR)/cJSON.o

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -f $(TARGET) $(OBJDIR)/*.o

.PHONY: all clean
