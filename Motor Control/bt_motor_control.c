/* ==============================================================================
 * File: bt_motor_control.c
 * Purpose: 
 * This program acts as the central brain for a Raspberry Pi 4 motor controller. 
 * It listens for commands over a Bluetooth connection from an Android app, 
 * reads physical hardware sensors to calculate motor speed (RPM), and adjusts 
 * motor power (PWM) based on the selected operating mode.
 *
 * Operating Modes:
 * 1. MANUAL: App sends raw PWM (power) values directly to the motor.
 * 2. MAINTAIN: App sends a Target RPM. The Pi uses an external assembly 
 * feedback loop to automatically adjust the PWM to hold that speed.
 * 3. SYNCED: The Pi reads the RPM of an *external* motor and dynamically 
 * sets that as the Target RPM, forcing the controlled motor to match it.
 *
 * Architecture / Flow:
 * - The main() function sets up the GPIO pins and establishes a Bluetooth socket.
 * - An outer while(1) loop waits for Bluetooth connections (allows reconnection).
 * - An inner while(1) loop polls sensors, calculates RPM, and runs feedback logic.
 * - It uses a non-blocking network check (select) so the program can instantly 
 * read incoming Bluetooth commands without freezing the motor control loop.
 * ============================================================================== */

#include <bcm2835.h>           // Raspberry Pi hardware control library
#include <stdio.h>             // Standard I/O (printf)
#include <stdlib.h>            // Standard library
#include <string.h>            // String manipulation (strtok, strncmp)
#include <unistd.h>            // POSIX operating system API (read, write, close)
#include <sys/time.h>          // Time tracking (gettimeofday) for the 1-second loop
#include <sys/socket.h>        // Core socket functions
#include <sys/select.h>        // Non-blocking I/O multiplexing (select)
#include <bluetooth/bluetooth.h> // Core Bluetooth library
#include <bluetooth/rfcomm.h>    // RFCOMM (Serial over Bluetooth) library

// --- Hardware Pin Configurations (BCM Numbering) ---
#define PWM_PIN 18         // Pin mapped to Hardware PWM Channel 0
#define DIR1_PIN 23        // Motor Driver Direction Pin A
#define DIR2_PIN 24        // Motor Driver Direction Pin B
#define IR_PIN 25          // Infrared sensor for the motor we are controlling
#define EXT_IR_PIN 19      // Infrared sensor for the external "master" motor
#define PWM_CHANNEL 0      // BCM2835 hardware PWM channel
#define PWM_RANGE 1024     // Resolution of the PWM (0 = 0% power, 1024 = 100% power)

// --- State Tracking Enum ---
typedef enum {
    MODE_MANUAL,
    MODE_MAINTAIN,
    MODE_SYNCED
} OperatingMode;

// --- Global State Variables ---
// These are global because both the main loop and the command parser need to read/modify them.
int is_running = 0;                  // 0 = stopped, 1 = motor active
int current_pwm = 0;                 // Current power level being sent to the motor (0-1024)
int target_rpm = 0;                  // Desired speed (used in MAINTAIN and SYNCED modes)
OperatingMode current_mode = MODE_MANUAL; // Defaults to manual control

// --- External Assembly Function Declaration ---
// This tells the C compiler that this function exists, but it's written in an external Assembly file.
extern int calculate_feedback_pwm(int target_rpm, int current_rpm, int current_pwm);

// --- Helper: Apply Hardware PWM ---
// A safety wrapper. It ensures that if the system state says the motor is OFF,
// it strictly forces the hardware PWM to 0, ignoring the current_pwm variable.
void apply_motor_state() {
    if (is_running) {
        bcm2835_pwm_set_data(PWM_CHANNEL, current_pwm);
    } else {
        bcm2835_pwm_set_data(PWM_CHANNEL, 0);
    }
}

// --- Parse Commands from Android ---
// This function takes the raw string received over Bluetooth and updates the system state.
void process_command(char *command) {
    // Commands are split by carriage returns/newlines (\r\n)
    char *token = strtok(command, "\r\n");
    
    while (token != NULL) {
        // App pressed the "START" button
        if (strncmp(token, "STATE:START", 11) == 0) {
            is_running = 1;
            
            // "Kickstart" Logic:
            // A dead motor requires a burst of power to break static friction. 
            // If we are supposed to be automatically holding a speed, but we are currently
            // sending 0 power, give it an immediate flat boost of 200/1024 power.
            if ((current_mode == MODE_MAINTAIN || current_mode == MODE_SYNCED) && current_pwm == 0 && target_rpm > 0) {
                current_pwm = 200; 
            }
            apply_motor_state(); // Push changes to hardware
        } 
        // App pressed the "STOP" button
        else if (strncmp(token, "STATE:STOP", 10) == 0) {
            is_running = 0;
            apply_motor_state();
        } 
        // App selected Forward direction
        else if (strncmp(token, "DIR:FORWARD", 11) == 0) {
            bcm2835_gpio_write(DIR1_PIN, HIGH);
            bcm2835_gpio_write(DIR2_PIN, LOW);
        } 
        // App selected Reverse direction
        else if (strncmp(token, "DIR:REVERSE", 11) == 0) {
            bcm2835_gpio_write(DIR1_PIN, LOW);
            bcm2835_gpio_write(DIR2_PIN, HIGH);
        }
        // App changed the Operating Mode
        else if (strncmp(token, "MODE:", 5) == 0) {
            if (strstr(token, "MANUAL")) current_mode = MODE_MANUAL;
            else if (strstr(token, "MAINTAIN")) current_mode = MODE_MAINTAIN;
            else if (strstr(token, "SYNCED")) current_mode = MODE_SYNCED;
        }
        // App sent a raw PWM slider value
        else if (strncmp(token, "PWM:", 4) == 0) {
            // Only respect manual PWM commands if we are actually in MANUAL mode
            if (current_mode == MODE_MANUAL) {
                sscanf(token, "PWM:%d", &current_pwm);
                
                // Clamp the values to prevent hardware damage/crashes
                if (current_pwm > 1000) current_pwm = 1000;
                if (current_pwm < 0) current_pwm = 0;
                
                apply_motor_state();
            }
        }
        // App sent a Target RPM value
        else if (strncmp(token, "TARGET_RPM:", 11) == 0) {
            // Ignore manual target inputs if Synced mode is running (Synced mode auto-generates targets)
            if (current_mode != MODE_SYNCED) {
                sscanf(token, "TARGET_RPM:%d", &target_rpm);
                
                // If we are actively running and a new target comes in, apply the kickstart
                if (current_mode == MODE_MAINTAIN && is_running && current_pwm == 0 && target_rpm > 0) {
                    current_pwm = 200; // Flat kickstart
                    apply_motor_state();
                }
                printf("\n[DEBUG] New Target RPM set to: %d\n", target_rpm);
            }
        }
        
        // Grab the next command in the string (if multiple were sent at once)
        token = strtok(NULL, "\r\n");
    }
}

int main() {
    // ---------------------------------------------------------
    // 1. Initialize bcm2835 Hardware & GPIO
    // ---------------------------------------------------------
    if (!bcm2835_init()) return 1; // Exit if library fails to load
    
    // Setup Motor Control Pins as Outputs
    bcm2835_gpio_fsel(DIR1_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(DIR2_PIN, BCM2835_GPIO_FSEL_OUTP);
    
    // Configure PWM Pin (Must use Alternate Function 5 for hardware PWM)
    bcm2835_gpio_fsel(PWM_PIN, BCM2835_GPIO_FSEL_ALT5);
    bcm2835_pwm_set_clock(BCM2835_PWM_CLOCK_DIVIDER_16); // Sets base clock frequency
    bcm2835_pwm_set_mode(PWM_CHANNEL, 1, 1);             // Enable Mark-Space mode
    bcm2835_pwm_set_range(PWM_CHANNEL, PWM_RANGE);       // Set 0-1024 resolution
    
    // Setup IR Sensors as Inputs (PUD_OFF disables internal pull-up resistors)
    bcm2835_gpio_fsel(IR_PIN, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(IR_PIN, BCM2835_GPIO_PUD_OFF);
    bcm2835_gpio_fsel(EXT_IR_PIN, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(EXT_IR_PIN, BCM2835_GPIO_PUD_OFF);
    
    // Initialize motor to spin Forward by default
    bcm2835_gpio_write(DIR1_PIN, HIGH);
    bcm2835_gpio_write(DIR2_PIN, LOW);

    // ---------------------------------------------------------
    // 2. Setup Bluetooth Server Socket
    // ---------------------------------------------------------
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    int s, client, bytes_read;
    socklen_t opt = sizeof(rem_addr);
    
    // Create an RFCOMM Bluetooth socket
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY; // Bind to any local Bluetooth adapter
    loc_addr.rc_channel = (uint8_t) 1; // Use RFCOMM Channel 1
    
    bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));
    listen(s, 1); // Listen for 1 incoming connection

    // --- RECONNECTION OUTER LOOP ---
    while (1) {
        printf("\nWaiting for Bluetooth Connection...\n");
        // This function BLOCKS the code until the Android app connects
        client = accept(s, (struct sockaddr *)&rem_addr, &opt);
        printf("Android Device Connected!\n");

        // ---------------------------------------------------------
        // 3. Main Loop Variables (Resets on new connection)
        // ---------------------------------------------------------
        struct timeval start, end;
        gettimeofday(&start, NULL); // Record loop start time
        
        // Pulse tracking for edge detection
        uint8_t last_ir_state = bcm2835_gpio_lev(IR_PIN);
        uint8_t last_ext_ir_state = bcm2835_gpio_lev(EXT_IR_PIN);
        int pulse_count = 0;       // Pulses on our motor
        int ext_pulse_count = 0;   // Pulses on external motor
        
        // Buffer for reading Bluetooth data
        char buffer[1024] = { 0 };
        fd_set readfds;
        struct timeval tv;

        // ---------------------------------------------------------
        // 4. High-Speed Control Loop
        // ---------------------------------------------------------
        while (1) {
            // --- IR Edge Detection (Our Motor) ---
            // We only count a pulse when the signal falls from HIGH to LOW.
            uint8_t current_ir_state = bcm2835_gpio_lev(IR_PIN);
            if (last_ir_state == HIGH && current_ir_state == LOW) {
                pulse_count++;
            }
            last_ir_state = current_ir_state;

            // --- IR Edge Detection (External Motor) ---
            uint8_t current_ext_ir_state = bcm2835_gpio_lev(EXT_IR_PIN);
            if (last_ext_ir_state == HIGH && current_ext_ir_state == LOW) {
                ext_pulse_count++;
            }
            last_ext_ir_state = current_ext_ir_state;

            // --- Timer Check ---
            // Calculate exactly how much time has passed since the last RPM check
            gettimeofday(&end, NULL);
            double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

            // ---------------------------------------------------------
            // 5. One-Second Interval Block (RPM Math & Feedback)
            // ---------------------------------------------------------
            if (elapsed >= 1.0) {
                // Math: (Pulses in 1 sec * 60 secs) / 3 slots per rotation = Revolutions Per Minute
                int current_rpm = (pulse_count * 60) / 3; 
                int external_rpm = (ext_pulse_count * 60) / 3;
                
                // If in Synced mode, the external motor dictates our target speed
                if (current_mode == MODE_SYNCED) {
                    target_rpm = external_rpm;
                }

                // Send live telemetry data back to the Android app
                char telemetry[128];
                snprintf(telemetry, sizeof(telemetry), "MEASURED_RPM:%d\nEXTERNAL_RPM:%d\n", current_rpm, external_rpm);
                
                // --- Check if write fails due to disconnect ---
                if (write(client, telemetry, strlen(telemetry)) < 0) {
                    printf("\nFailed to send telemetry. Connection lost!\n");
                    break; // Break inner loop to trigger reconnection
                }

                // --- Assembly Feedback Loop Application ---
                // Only run the feedback loop if we are actively trying to hold a speed
                if ((current_mode == MODE_MAINTAIN || current_mode == MODE_SYNCED) && is_running) {
                    
                    // Safety feature: If target drops to 0, kill power to prevent runaway loop
                    if (target_rpm == 0) {
                        current_pwm = 0;
                        apply_motor_state();
                        
                        if (current_mode == MODE_SYNCED) {
                            printf("\r[SYNC] Ext RPM is 0. Waiting for external motor to move...     ");
                        } else {
                            printf("\r[MAINTAIN] Target is 0. Waiting for target...                  ");
                        }
                    } else {
                        // Safe jumpstart: Break static friction if starting from a dead stop
                        if (current_pwm == 0) {
                            current_pwm = 200; 
                        }
                        
                        // Execute external Assembly logic to calculate the new PWM
                        current_pwm = calculate_feedback_pwm(target_rpm, current_rpm, current_pwm);
                        apply_motor_state(); 
                        
                        // Print local console telemetry
                        if (current_mode == MODE_SYNCED) {
                            printf("\r[SYNC] Ext RPM: %d | My RPM: %d | Adj PWM: %d    ", external_rpm, current_rpm, current_pwm);
                        } else {
                            printf("\r[MAINTAIN] Target: %d | My RPM: %d | Adj PWM: %d    ", target_rpm, current_rpm, current_pwm);
                        }
                    }
                } else {
                    // Not using feedback, just print what we are doing manually
                    printf("\r[MANUAL] RPM: %d | Ext RPM: %d | PWM: %d    ", current_rpm, external_rpm, current_pwm);
                }
                fflush(stdout); // Force printf to update the same terminal line

                // Reset variables for the next 1-second window
                pulse_count = 0;
                ext_pulse_count = 0;
                gettimeofday(&start, NULL);
            }

            // ---------------------------------------------------------
            // 6. Non-Blocking Network Check
            // ---------------------------------------------------------
            // We use 'select' with a timeout of 0 (tv.tv_sec = 0; tv.tv_usec = 0).
            // This allows us to peek at the Bluetooth socket. If there is a command waiting, 
            // we read it. If it is empty, the code immediately moves on without pausing.
            FD_ZERO(&readfds);
            FD_SET(client, &readfds);
            tv.tv_sec = 0;
            tv.tv_usec = 0;

            if (select(client + 1, &readfds, NULL, NULL, &tv) > 0) {
                memset(buffer, 0, sizeof(buffer));
                bytes_read = read(client, buffer, sizeof(buffer));
                
                if (bytes_read > 0) {
                    // Command received, send to parser
                    process_command(buffer);
                } else {
                    // If select() triggers but 0 bytes are read, the connection was lost.
                    printf("\nConnection lost! Shutting down motor.\n");
                    bcm2835_pwm_set_data(PWM_CHANNEL, 0); // Fail-safe motor shutdown
                    break; // Exit the inner while loop to trigger reconnection
                }
            }

            // Brief delay to prevent the CPU from hitting 100% usage while polling sensors
            bcm2835_delayMicroseconds(100); 
        } // End of inner high-speed control loop

        // --- SESSION CLEANUP ---
        // Reset global state for the next connection so the motor doesn't resume old commands
        is_running = 0;
        current_pwm = 0;
        target_rpm = 0;
        current_mode = MODE_MANUAL;
        bcm2835_pwm_set_data(PWM_CHANNEL, 0); // Safely kill motor power
        close(client); // Close ONLY the client socket

    } // End of outer reconnection loop

    // ---------------------------------------------------------
    // 7. Final Cleanup (Only reached if program is killed)
    // ---------------------------------------------------------
    close(s);            // Close server socket
    bcm2835_close();     // Release hardware pins
    return 0;
}