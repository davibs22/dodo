CXX = g++
CC = gcc
CXXFLAGS = -std=c++11 -Wall
CFLAGS = -Wall -Isrc
GLIBFLAGS = `pkg-config --cflags glib-2.0 gio-2.0`
GTKFLAGS = `pkg-config --cflags --libs gtk+-3.0`
TARGET = dist/dodo
CPP_SOURCE = main.cpp

CORE_SOURCES = src/core/runtime/command.c \
               src/core/container_state.c \
               src/core/parse/docker_output.c \
               src/core/parse/json_format.c \
               src/core/repository/container_repo.c \
               src/core/repository/image_repo.c \
               src/core/repository/network_repo.c \
               src/core/repository/volume_repo.c \
               src/core/service/container_service.c \
               src/core/service/compose_service.c \
               src/core/service/image_service.c \
               src/core/service/network_service.c \
               src/core/service/volume_service.c \
               src/core/service/stats_service.c

GTK_SOURCES = src/interface/gtk/utils/status_utils.c \
              src/interface/gtk/models/container.c \
              src/interface/gtk/models/image.c \
              src/interface/gtk/models/network.c \
              src/interface/gtk/models/volume.c \
              src/interface/gtk/containers_table.c \
              src/interface/gtk/images_table.c \
              src/interface/gtk/networks_table.c \
              src/interface/gtk/volumes_table.c \
              src/interface/gtk/window.c

C_SOURCES = $(CORE_SOURCES) $(GTK_SOURCES)
OBJECTS = $(C_SOURCES:.c=.o)
CORE_OBJECTS = $(CORE_SOURCES:.c=.o)
GTK_OBJECTS = $(GTK_SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(CPP_SOURCE) $(OBJECTS)
	mkdir -p dist
	$(CXX) $(CXXFLAGS) -Isrc -o $(TARGET) $(CPP_SOURCE) $(OBJECTS) $(GTKFLAGS) -lm

src/core/%.o: src/core/%.c
	$(CC) $(CFLAGS) $(GLIBFLAGS) -c $< -o $@

src/interface/gtk/%.o: src/interface/gtk/%.c
	$(CC) $(CFLAGS) $(GTKFLAGS) -c $< -o $@

clean:
	rm -rf dist $(OBJECTS)

deb:
	dpkg-buildpackage -us -uc -b

.PHONY: all clean deb
