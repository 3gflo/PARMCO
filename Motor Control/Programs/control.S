// control.S
.global adjust_pwm
.align 2

// C-Signature: 
// int adjust_pwm(int current_rpm, int target_rpm, int current_pwm, int pwm_range);
// 
// Register Mapping:
// r0 = current_rpm
// r1 = target_rpm
// r2 = current_pwm
// r3 = pwm_range

adjust_pwm:
    // 1. Check if the target RPM is "Low" (< 1500)
    // 1500 is too large for an immediate value, so we load it into r12 first
    ldr r12, =1500      
    cmp r1, r12

    // 2. Calculate Error: r1 = Target RPM - Current RPM
    // (We do this after the compare, but before we branch)
    sub r1, r1, r0      

    // 3. Branch based on our earlier target check
    blt gentle_gain     // Branch to gentle_gain if Target < 1500

aggressive_gain:
    // For speeds >= 1500: divide error by 16
    asr r1, r1, #4      
    b apply_pwm         // Skip over the gentle math

gentle_gain:
    // For speeds < 1500: divide error by 32 for smoother low-end control
    asr r1, r1, #5      

apply_pwm:
    // 4. Calculate New PWM: Current PWM + Adjustment
    add r0, r2, r1      

    // 5. Clamp to minimum (0) 
    cmp r0, #0
    movlt r0, #0

    // 6. Clamp to maximum (pwm_range)
    cmp r0, r3
    movgt r0, r3

    // 7. Return to C
    bx lr