#include <bcm2835.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

// BCM Pin Definitions
#define PWM_PIN     18
#define DIR1_PIN    23
#define DIR2_PIN    24

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

    // 1. Setup Direction Pins
    bcm2835_gpio_fsel(DIR1_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(DIR2_PIN, BCM2835_GPIO_FSEL_OUTP);

    // 2. Setup Hardware PWM
    bcm2835_gpio_fsel(PWM_PIN, BCM2835_GPIO_FSEL_ALT5);
    bcm2835_pwm_set_clock(BCM2835_PWM_CLOCK_DIVIDER_16);
    bcm2835_pwm_set_mode(PWM_CHANNEL, 1, 1);
    bcm2835_pwm_set_range(PWM_CHANNEL, PWM_RANGE);

    stop_motor(); // Ensure safe start state

    printf("\n--- Motor Deadband Calibration --- \n");
    printf("The motor will now slowly increase its PWM signal.\n");
    printf("Watch the motor shaft closely.\n");
    printf("-> Press ANY KEY the exact moment the motor starts spinning reliably.\n\n");
    
    // Give the user 3 seconds to get ready before the sweep starts
    for(int i = 3; i > 0; i--) {
        printf("Starting in %d...\r", i);
        fflush(stdout);
        bcm2835_delay(1000);
    }
    printf("Sweeping...            \n");

    // Set motor to drive Forward
    bcm2835_gpio_write(DIR1_PIN, HIGH);
    bcm2835_gpio_write(DIR2_PIN, LOW);

    int min_pwm = 0;
    
    // Sweep loop: Increase PWM by 5 every 150 milliseconds
    for (int i = 0; i <= PWM_RANGE; i += 5) {
        bcm2835_pwm_set_data(PWM_CHANNEL, i);
        printf("\rTesting PWM: %4d", i);
        fflush(stdout);

        if (kbhit()) {
            getchar(); // Clear the key from the buffer
            min_pwm = i;
            break;
        }
        
        bcm2835_delay(150); 
    }

    // Stop the hardware and clean up
    stop_motor();
    bcm2835_close();

    if (min_pwm > 0) {
        printf("\n\nCalibration Complete!\n");
        printf("-> Your motor's minimum starting PWM is approximately: %d\n", min_pwm);
        printf("-> Update the #define MIN_PWM in your main C file with this number.\n\n");
    } else {
        printf("\n\nSweep finished or interrupted before motion was detected.\n");
    }

    return 0;
}