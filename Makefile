CC = gcc
CFLAGS = -Wall -Wextra -std=c23 -pedantic
INCLUDES = -Iinclude
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Source files
SOURCES = $(wildcard $(SRCDIR)/core/*.c) \
          $(wildcard $(SRCDIR)/util/*.c) \
          $(SRCDIR)/main.c

# Object files
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Make sure the object files are in the correct directories
$(shell mkdir -p $(OBJDIR)/core $(OBJDIR)/util $(BINDIR))

# Main target
TARGET = $(BINDIR)/plike

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

# Pattern rule for object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Debug builds
debug: CFLAGS += -g -DDEBUG
debug: all