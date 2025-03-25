# Makefile for NT Rigue
# A vertical scrolling shooter game for Windows NT using GDI graphics

# Compiler settings for Visual Studio 4.0
CC = cl.exe
CFLAGS = /nologo /O2
LIBS = gdi32.lib user32.lib winmm.lib

# Target executable
TARGET = ntrigue.exe

# Build rule
$(TARGET): ntrigue.c
	$(CC) $(CFLAGS) ntrigue.c /Fe$(TARGET) $(LIBS)

# Clean rule
clean:
	-del $(TARGET)

# Default target
all: $(TARGET)