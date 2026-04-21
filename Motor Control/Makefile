# Compiler and Flags
CC = gcc
CFLAGS = -Wall -O2

# Required External Libraries
# -lbcm2835: For GPIO and Hardware PWM
# -lbluetooth: For RFCOMM socket communication
LDFLAGS = -lbcm2835 -lbluetooth

# File Names
TARGET = bt_motor_control
C_SRC = bt_motor_control.c
ASM_SRC = feedback.S

# Combine both C and Assembly files for the build
SRC = $(C_SRC) $(ASM_SRC)

# Default rule when you just type 'make'
all: $(TARGET)

# Compilation rule
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

# Rule to compile (if needed) and run with root privileges
run: $(TARGET)
	sudo ./$(TARGET)

# Rule to clean up the compiled executable
clean:
	rm -f $(TARGET)