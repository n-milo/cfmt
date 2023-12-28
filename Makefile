EXEC=exec
CFLAGS=-g3 -O0 -fsanitize=address -Wall -Wextra -Wconversion -fmacro-backtrace-limit=1

all: $(EXEC)

run: $(EXEC)
	./$(EXEC)

clean:
	rm *.o $(EXEC)

$(EXEC): example.o
	$(CC) $(CFLAGS) $^ -o $@

example.o: format.h macros.h
