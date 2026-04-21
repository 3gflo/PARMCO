/* ==============================================================================
 * File: feedback.S
 * Architecture: 32-bit ARM (AArch32)
 * Purpose: 
 * This function is an Adaptive Proportional (P) controller. It calculates the 
 * necessary adjustment to a motor's power (PWM) to make its current RPM match 
 * a target RPM. 
 *
 * Register Map (ARM Calling Convention):
 * - r0 : Input 1 (target_rpm)  | Output (new_pwm)
 * - r1 : Input 2 (current_rpm) | Reused as Absolute Error
 * - r2 : Input 3 (current_pwm) 
 * - r3 : Used to store the Raw Error (Target - Current)
 * - r12: Used to store the dynamic bit-shift value (the "P-Gain")
 *
 * How it works:
 * 1. Error Calculation: It finds the difference between the Target and Current RPM.
 * 2. Adaptive Gain: Instead of a flat division, it uses dynamic bit-shifting based 
 * on the size of the error. 
 * - Massive error? Divide by 8 (Shift 3) for a hard, fast correction.
 * - Tiny error (<100)? Divide by 64 (Shift 6) for a gentle nudge. This acts 
 * as a "deadband" that prevents the motor from violently oscillating back 
 * and forth when it is very close to the target speed.
 * 3. Low-Speed Dampening: If the target speed is low (<1000 RPM), it adds an extra
 * shift (divides by an extra factor of 2) to prevent over-torquing at low speeds.
 * 4. Safety Clamping: It ensures the final PWM value never drops below 0 or exceeds
 * the maximum limit of 1000 before returning it to the C program.
 * ============================================================================== */

.text
.global calculate_feedback_pwm
.align 2

calculate_feedback_pwm:
    // 1. Calculate Raw Error
    sub r3, r0, r1          // r3 = target_rpm - current_rpm

    // 2. Absolute Value of Error (Store in r1)
    cmp r3, #0
    movge r1, r3            
    rsblt r1, r3, #0        

    // 3. Granular Adaptive Shift (More gears for high RPM)
    mov r12, #3             // Default: Shift 3 (Divide Error by 8)
    
    cmp r1, #600
    movlt r12, #4           // If error < 600: Shift 4 (Divide by 16)
    
    cmp r1, #300
    movlt r12, #5           // If error < 300: Shift 5 (Divide by 32)
    
    cmp r1, #100
    movlt r12, #6           // If error < 100: Shift 6 (Divide by 64) -> Creates stable deadband

    // 4. Low RPM Dampening
    ldr r1, =1000           
    cmp r0, r1              
    addlt r12, r12, #1      // If target_rpm < 1000, dampen by an extra shift layer

    // 5. Apply the Shift to Original Error
    asr r3, r3, r12         // r3 = error / (2 ^ r12)

    // 6. Calculate New PWM
    add r0, r2, r3          // r0 = current_pwm + adjusted_error

    // 7. Clamp Ceiling (Max 1000)
    ldr r1, =1000           
    cmp r0, r1              
    movgt r0, r1            

    // 8. Clamp Floor (Min 0)
    cmp r0, #0
    movlt r0, #0            

    bx lr