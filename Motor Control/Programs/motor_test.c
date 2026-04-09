#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h> 

// BCM Pin Definitions
#define PWM_PIN     18
#define DIR1_PIN    23
#define DIR2_PIN    24
#define IR_PIN      25

#define PWM_CHANNEL 0
#define PWM_RANGE   1024

// --- LIMITS ---
#define MIN_PWM     350   // Adjusted minimum PWM 
#define MAX_RPM     5000  // Sets a hard ceiling to prevent dangerous runaway inputs

extern int adjust_pwm(int current_rpm, int target_rpm, int current_pwm, int pwm_range);

void set_nonblocking() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void set_blocking() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

void stop_motor() {
    bcm2835_gpio_write(DIR1_PIN, LOW);
    bcm2835_gpio_write(DIR2_PIN, LOW);
    bcm2835_pwm_set_data(PWM_CHANNEL, 0);
}

int main(int argc, char **argv) {
    if (!bcm2835_init()) {
        printf("bcm2835_init failed. Are you root?\n");
        return 1;
    }

    bcm2835_gpio_fsel(DIR1_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(DIR2_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(IR_PIN, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(IR_PIN, BCM2835_GPIO_PUD_OFF); 
    bcm2835_gpio_fen(IR_PIN); 

    bcm2835_gpio_fsel(PWM_PIN, BCM2835_GPIO_FSEL_ALT5);
    bcm2835_pwm_set_clock(BCM2835_PWM_CLOCK_DIVIDER_16);
    bcm2835_pwm_set_mode(PWM_CHANNEL, 1, 1);
    bcm2835_pwm_set_range(PWM_CHANNEL, PWM_RANGE);

    int target_rpm = 1000;     
    int current_pwm = 0;       
    int current_direction = 0; 
    
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);
    int pulse_count = 0;

    stop_motor(); 
    set_nonblocking(); 

    printf("\n--- Advanced Closed-Loop Motor Control ---\n");
    printf(" 'w' : Forward\n 's' : Reverse\n 'x' : Stop\n");
    printf(" 'r' : Manually Type Target RPM\n");
    printf(" '=' : Target RPM +100\n '-' : Target RPM -100\n 'q' : Quit\n\n");

    while (1) {
        if (bcm2835_gpio_eds(IR_PIN)) {
            pulse_count++;
            bcm2835_gpio_set_eds(IR_PIN); 
        }

        gettimeofday(&current_time, NULL);
        double elapsed = (current_time.tv_sec - start_time.tv_sec) + 
                         (current_time.tv_usec - start_time.tv_usec) / 1000000.0;
                         
        if (elapsed >= 1.0) {
            int current_rpm = (pulse_count * 60) / 3; 
            
            if (current_direction != 0) {
                // Call Assembly feedback
                current_pwm = adjust_pwm(current_rpm, target_rpm, current_pwm, PWM_RANGE);
                
                // DEADBAND COMPENSATION: Never let the PWM drop below the stall limit if trying to spin
                if (target_rpm > 0 && current_pwm < MIN_PWM) {
                    current_pwm = MIN_PWM;
                }
                
                bcm2835_pwm_set_data(PWM_CHANNEL, current_pwm);
            } else {
                current_pwm = 0; 
            }
            
            printf("\r[LIVE] Target RPM: %4d | Actual: %4d | PWM: %4d | State: %s    ", 
                   target_rpm, current_rpm, current_pwm, 
                   (current_direction == 0) ? "STOP" : (current_direction == 1) ? "FWD " : "REV ");
            fflush(stdout); 
            
            pulse_count = 0; 
            start_time = current_time;
        }

        int c = getchar();
        if (c != EOF) {
            if (c == 'q') {
                break;
            } else if (c == 'w') {
                current_direction = 1;
                bcm2835_gpio_write(DIR1_PIN, HIGH);
                bcm2835_gpio_write(DIR2_PIN, LOW);
            } else if (c == 's') {
                current_direction = 2;
                bcm2835_gpio_write(DIR1_PIN, LOW);
                bcm2835_gpio_write(DIR2_PIN, HIGH);
            } else if (c == 'x') {
                current_direction = 0;
                stop_motor(); 
            } else if (c == '=') {
                target_rpm += 100;
                if (target_rpm > MAX_RPM) target_rpm = MAX_RPM; 
            } else if (c == '-') {
                target_rpm -= 100;
                if (target_rpm < 0) target_rpm = 0; 
            } else if (c == 'r') {
                stop_motor();
                
                set_blocking();
                printf("\n\nEnter new Target RPM (Max %d): ", MAX_RPM);
                
                if (scanf("%d", &target_rpm) != 1) {
                    printf("\nInvalid input. Ignoring.\n");
                }
                
                int ch;
                while((ch = getchar()) != '\n' && ch != EOF); 
                
                if (target_rpm < 0) {
                    target_rpm = 0;
                } else if (target_rpm > MAX_RPM) {
                    printf("Warning: %d RPM exceeds max limit. Capping to %d.\n", target_rpm, MAX_RPM);
                    target_rpm = MAX_RPM;
                }
                
                set_nonblocking();
                
                if (current_direction == 1) {
                    bcm2835_gpio_write(DIR1_PIN, HIGH);
                    bcm2835_gpio_write(DIR2_PIN, LOW);
                } else if (current_direction == 2) {
                    bcm2835_gpio_write(DIR1_PIN, LOW);
                    bcm2835_gpio_write(DIR2_PIN, HIGH);
                }
                
                current_pwm = MIN_PWM; 
                
                printf("-> Target RPM set to %d. Resuming control...\n\n", target_rpm);
            }
        }
        bcm2835_delay(1); 
    }

    stop_motor();
    set_blocking(); 
    bcm2835_close();
    printf("\nExited Safely.\n");
    return 0;
}