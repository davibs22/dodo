CXX = g++
CC = gcc
CXXFLAGS = -std=c++11 -Wall
CFLAGS = -Wall
GTKFLAGS = `pkg-config --cflags --libs gtk+-3.0`
TARGET = dodo
CPP_SOURCE = main.cpp

# Lista todos os arquivos C fonte
C_SOURCES = src/docker/docker_command.c \
            src/utils/status_utils.c \
            src/models/container.c \
            src/models/image.c \
            src/models/network.c \
            src/models/volume.c \
            src/ui/containers_table.c \
            src/ui/images_table.c \
            src/ui/networks_table.c \
            src/ui/volumes_table.c \
            src/ui/window.c

OBJECTS = $(C_SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(CPP_SOURCE) $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(CPP_SOURCE) $(OBJECTS) $(GTKFLAGS) -lm

%.o: %.c
	$(CC) $(CFLAGS) $(GTKFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
