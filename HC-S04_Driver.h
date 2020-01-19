/*
 * HC-S04_Driver.h
 *
 *  Created on: Dec 23, 2019
 *      Author: Max
 */

#ifndef HC_S04_DRIVER_H_
#define HC_S04_DRIVER_H_

#define NVIC_PRI5_TIME1AB_MASK 0xFF0000FF
#define SPEED_SOUND 34300          // Speed of sound in cm/s
#define CLOCK_TICK  0.00000008     // Clock tick time for 12.5MHz oscillator

void Timer1AB_Init(void);
void TriggerSignalEnable(void);
void EnableEdgeModeTimers(void);
void UART_Init(void);
void UART_OutChar(unsigned char data);
void UART_OutUDec(unsigned long n);
void Distance_Measure_Enable(void);
void Restart_measurement(void);
void Initialize_HCS04(void);

#endif /* HC_S04_DRIVER_H_ */
