#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h> // Required for accurate RPM timing intervals

// BCM Pin Definitions
#define PWM_PIN     18
#define DIR1_PIN    23
#define DIR2_PIN    24
#define IR_PIN      25

#define PWM_CHANNEL 0
#define PWM_RANGE   1024

// Function to allow non-blocking keyboard input
int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

void stop_motor() {
    bcm2835_gpio_write(DIR1_PIN, LOW);
    bcm2835_gpio_write(DIR2_PIN, LOW);
    bcm2835_pwm_set_data(PWM_CHANNEL, 0);
}

int main(int argc, char **argv) {
    if (!bcm2835_init()) {
        printf("bcm2835_init failed. Are you running as root?\n");
        return 1;
    }

    // 1. Setup Pins
    bcm2835_gpio_fsel(DIR1_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(DIR2_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(IR_PIN, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(IR_PIN, BCM2835_GPIO_PUD_OFF); 

    // 2. Setup Hardware PWM
    bcm2835_gpio_fsel(PWM_PIN, BCM2835_GPIO_FSEL_ALT5);
    bcm2835_pwm_set_clock(BCM2835_PWM_CLOCK_DIVIDER_16);
    bcm2835_pwm_set_mode(PWM_CHANNEL, 1, 1);
    bcm2835_pwm_set_range(PWM_CHANNEL, PWM_RANGE);

    // State Variables
    int current_speed = 0;     // Acts as the saved "Throttle"
    int current_direction = 0; // 0 = Stopped, 1 = FWD, 2 = REV
    
    // RPM Calculation Variables
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);
    int pulse_count = 0;
    uint8_t last_ir_state = bcm2835_gpio_lev(IR_PIN);

    stop_motor(); // Ensure safe start state

    printf("\n--- Motor Control & RPM Validation ---\n");
    printf(" 'w' : Forward (Resumes saved throttle)\n");
    printf(" 's' : Reverse (Resumes saved throttle)\n");
    printf(" 'x' : Stop (Maintains throttle setting)\n");
    printf(" '=' : Speed Up\n '-' : Speed Down\n 'q' : Quit\n\n");

    while (1) {
        // --- 1. SENSOR POLLING & EDGE DETECTION ---
        uint8_t current_ir_state = bcm2835_gpio_lev(IR_PIN);
        
        // Flipped Logic: HIGH = Clear, LOW = Covered. 
        // We trigger a count exactly when it transitions from Clear to Covered.
        if (last_ir_state == HIGH && current_ir_state == LOW) {
            pulse_count++;
        }
        last_ir_state = current_ir_state;

        // --- 2. RPM CALCULATION (Every 1 Second) ---
        gettimeofday(&current_time, NULL);
        double elapsed = (current_time.tv_sec - start_time.tv_sec) + 
                         (current_time.tv_usec - start_time.tv_usec) / 1000000.0;
                         
        if (elapsed >= 1.0) {
            // 3 pulses = 1 revolution. (pulses / 3) * 60 seconds = RPM
            int rpm = (pulse_count * 60) / 3; 
            
            // \r overwrites the current terminal line to create a static dashboard
            printf("\r[LIVE] Throttle: %4d / %4d | State: %s | RPM: %4d    ", 
                   current_speed, PWM_RANGE, 
                   (current_direction == 0) ? "STOP" : (current_direction == 1) ? " FWD" : " REV", 
                   rpm);
            fflush(stdout); // Force the terminal to draw the line immediately
            
            pulse_count = 0; // Reset counter for the next second
            start_time = current_time;
        }

        // --- 3. KEYBOARD INPUT HANDLING ---
        if (kbhit()) {
            char c = getchar();
            
            if (c == 'q') {
                printf("\n\nExiting safely...\n");
                break;
            } else if (c == 'w') {
                current_direction = 1;
                bcm2835_gpio_write(DIR1_PIN, HIGH);
                bcm2835_gpio_write(DIR2_PIN, LOW);
                bcm2835_pwm_set_data(PWM_CHANNEL, current_speed);
                printf("\n-> Drive: FORWARD\n");
            } else if (c == 's') {
                current_direction = 2;
                bcm2835_gpio_write(DIR1_PIN, LOW);
                bcm2835_gpio_write(DIR2_PIN, HIGH);
                bcm2835_pwm_set_data(PWM_CHANNEL, current_speed);
                printf("\n-> Drive: REVERSE\n");
            } else if (c == 'x') {
                current_direction = 0;
                stop_motor(); // Stops motor but DOES NOT erase current_speed
                printf("\n-> Motor STOPPED (Throttle maintained at %d)\n", current_speed);
            } else if (c == '=') {
                current_speed += 100;
                if (current_speed > PWM_RANGE) current_speed = PWM_RANGE;
                // If the motor is currently driving, instantly apply the new speed
                if (current_direction != 0) {
                    bcm2835_pwm_set_data(PWM_CHANNEL, current_speed);
                }
                printf("\n-> Throttle UP: %d\n", current_speed);
            } else if (c == '-') {
                current_speed -= 100;
                if (current_speed < 0) current_speed = 0;
                if (current_direction != 0) {
                    bcm2835_pwm_set_data(PWM_CHANNEL, current_speed);
                }
                printf("\n-> Throttle DOWN: %d\n", current_speed);
            }
        }
        
        // 1 millisecond delay allows polling the sensor at ~1000 Hz.
        // This is fast enough to accurately read up to 20,000 RPM.
        bcm2835_delay(1); 
    }

    // Cleanup before exit
    stop_motor();
    bcm2835_close();
    return 0;
}