#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

// Global variables to simulate the motor spinning up/down for testing
int current_target_rpm = 0;
int simulated_measured_rpm = 0;

void process_command(char *raw_buffer) {
    // TCP/RFCOMM can sometimes clump messages together. 
    // strtok splits the buffer by newlines so we don't miss any commands.
    char *command = strtok(raw_buffer, "\r\n");
    
    while (command != NULL) {
        printf("Received Command: %s\n", command);

        // 1. Handle State Commands
        if (strcmp(command, "STATE:START") == 0) {
            printf("--> Action: Turning motor ON\n");
        } 
        else if (strcmp(command, "STATE:STOP") == 0) {
            printf("--> Action: Turning motor OFF\n");
            current_target_rpm = 0; // Stop motor simulation
        }
        // 2. Handle Direction Commands
        else if (strcmp(command, "DIR:REVERSE") == 0) {
            printf("--> Action: Motor REVERSED\n");
        } 
        else if (strcmp(command, "DIR:FORWARD") == 0) {
            printf("--> Action: Motor FORWARD\n");
        }
        // 3. Handle Mode Commands
        else if (strcmp(command, "MODE:SYNCED") == 0) {
            printf("--> Action: IR Sensor Mode ACTIVATED\n");
        } 
        else if (strcmp(command, "MODE:MANUAL") == 0) {
            printf("--> Action: Manual Mode ACTIVATED\n");
        }
        else if (strcmp(command, "MODE:MAINTAIN") == 0) {
            printf("--> Action: Maintain Mode ACTIVATED\n");
        }
        // 4. Handle RPM Commands (Now looking for "RPM:" instead of "SPEED:")
        else if (strncmp(command, "RPM:", 4) == 0) {
            int rpm_val;
            if (sscanf(command, "RPM:%d", &rpm_val) == 1) {
                printf("--> Action: Setting target RPM to %d\n", rpm_val);
                current_target_rpm = rpm_val; // Update target for simulation
            }
        }
        
        // Get the next command in the buffer (if any)
        command = strtok(NULL, "\r\n");
    }
}

int main() {
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[1024] = { 0 };
    int s, client, bytes_read;
    socklen_t opt = sizeof(rem_addr);

    while (1) {
        // 1. Create a Bluetooth RFCOMM socket
        s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
        if (s < 0) {
            perror("Failed to create socket");
            exit(1);
        }

        // 2. Bind socket to port 1
        loc_addr.rc_family = AF_BLUETOOTH;
        loc_addr.rc_bdaddr = *BDADDR_ANY;
        loc_addr.rc_channel = (uint8_t) 1;
        bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));

        // 3. Put socket into listening mode
        listen(s, 1);
        printf("\nRaspberry Pi Bluetooth Server is running...\n");
        printf("Waiting for connection from your Android app...\n");

        // 4. Accept connection
        client = accept(s, (struct sockaddr *)&rem_addr, &opt);
        if (client < 0) {
            perror("Failed to accept connection");
            close(s);
            continue;
        }

        char client_bt_address[18] = { 0 };
        ba2str(&rem_addr.rc_bdaddr, client_bt_address);
        printf("Success! Connected to: %s\n", client_bt_address);

        // 5. Read/Write Loop with Timeout
        while (1) {
            fd_set readfds;
            struct timeval tv;

            FD_ZERO(&readfds);
            FD_SET(client, &readfds);

            // Set timeout to 0.25 seconds (250,000 microseconds)
            tv.tv_sec = 0;
            tv.tv_usec = 250000; 

            // select() waits for data to arrive OR for the timeout to trigger
            int activity = select(client + 1, &readfds, NULL, NULL, &tv);

            if (activity < 0) {
                perror("Select error");
                break;
            }

            // Check if there is incoming data from the app to read
            if (activity > 0 && FD_ISSET(client, &readfds)) {
                memset(buf, 0, sizeof(buf)); 
                bytes_read = read(client, buf, sizeof(buf) - 1);

                if (bytes_read > 0) {
                    process_command(buf);
                } else if (bytes_read <= 0) {
                    printf("Client disconnected.\n");
                    break; // Exit inner loop
                }
            }

            // --- TELEMETRY SENDING LOGIC ---
            
            // Simulate the motor catching up to the target RPM
            if (simulated_measured_rpm < current_target_rpm) {
                simulated_measured_rpm += 15;
                if (simulated_measured_rpm > current_target_rpm) simulated_measured_rpm = current_target_rpm;
            } else if (simulated_measured_rpm > current_target_rpm) {
                simulated_measured_rpm -= 15;
                if (simulated_measured_rpm < current_target_rpm) simulated_measured_rpm = current_target_rpm;
            }

            // Format the string exactly how the Android app expects it
            char telemetry_msg[50];
            snprintf(telemetry_msg, sizeof(telemetry_msg), "MEASURED_RPM:%d\n", simulated_measured_rpm);
            
            // Send it over Bluetooth
            int bytes_written = write(client, telemetry_msg, strlen(telemetry_msg));
            if (bytes_written < 0) {
                printf("Failed to write to client. Disconnecting.\n");
                break;
            }
        }

        // 6. Clean up
        close(client);
        close(s);
    }

    return 0;
}