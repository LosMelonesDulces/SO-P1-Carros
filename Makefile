# Compilador y flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11

# Archivos fuente
SRCS = main.c algoritmos_calendarizacion.c cethreads.c simulacion.c

# Archivos objeto (reemplaza .c por .o)
OBJS = $(SRCS:.c=.o)

# Ejecutable
TARGET = program

# Regla por defecto
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Regla para archivos .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpiar archivos generados
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
