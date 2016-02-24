CC= gcc
FLAG= -std=c99
SOURCE= mpc.c lisp.c
TARGET= lisp
LIB= -ledit

all:
	$(CC) $(FLAG) -o $(TARGET) $(SOURCE) $(LIB)

clean:
	rm -rf $(TARGET)
