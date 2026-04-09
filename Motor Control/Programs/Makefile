# --- Compiler and Flags ---
CC = gcc
CFLAGS = -Wall -O2
LIBS = -lbcm2835

# --- Files and Target ---
TARGET = motortest
OBJS = motor_test.o control.o

# --- Default Build Rule ---
all: $(TARGET)

# --- Linking the final executable ---
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# --- Compiling the C file into an object file ---
motor_test.o: motor_test.c
	$(CC) $(CFLAGS) -c motor_test.c -o motor_test.o

# --- Compiling the Assembly file into an object file ---
control.o: control.S
	$(CC) $(CFLAGS) -c control.S -o control.o

# --- Custom Rule to Compile and Immediately Run as Root ---
run: $(TARGET)
	sudo ./$(TARGET)

# --- Cleanup Rule ---
clean:
	rm -f $(OBJS) $(TARGET)