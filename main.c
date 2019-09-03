#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "inc/tm4c123gh6pm.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "inc/hw_gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"

#define NVIC_PRI5_TIME1AB_MASK 0xFF0000FF
#define SPEED_SOUND 34300          // Speed of sound in cm/s
#define CLOCK_TICK  0.00000008     // Clock tick time for 12.5MHz oscillator

uint16_t hi;
uint16_t pulseWidth = 0; // Needs to be this data type to be congruent with register values.
uint16_t risingEdge;
uint16_t distanceCM;
void Timer1AB_Init(void);
void TriggerSignalEnable(void);
void EnableEdgeModeTimers(void);
void UART_Init(void);
void UART_OutChar(unsigned char data);
void UART_OutUDec(unsigned long n);

unsigned int overFlowCount;
unsigned int risingEdgeMeasured = 0;
int measured = 0;
int count;
int main(void)
{
    SysCtlClockSet(SYSCTL_SYSDIV_16| SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ |SYSCTL_OSC_MAIN); //Set up clock to run at 12.5MHz
    EnableEdgeModeTimers();
    UART_Init();
    TriggerSignalEnable();
    Timer1AB_Init();
    IntMasterEnable();
    while(1){
        ++count;
    }
    return 0;
}

void UART_Init(void){
  SYSCTL_RCGC1_R |= SYSCTL_RCGC1_UART0; // activate UART0
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOA; // activate port A
  UART0_CTL_R &= ~UART_CTL_UARTEN;      // disable UART
  UART0_IBRD_R = 6;                    // IBRD = int(12,500,000 / (16 * 115,200)) = int(6.7816)
  UART0_FBRD_R = 50;                     // FBRD = int(0.7816 * 64 + 0.5) = 8
                                        // 8 bit word length (no parity bits, one stop bit, FIFOs)
  UART0_LCRH_R = (UART_LCRH_WLEN_8|UART_LCRH_FEN);
  UART0_CTL_R |= UART_CTL_UARTEN;       // enable UART
  GPIO_PORTA_AFSEL_R |= 0x03;           // enable alt funct on PA1-0
  GPIO_PORTA_DEN_R |= 0x03;             // enable digital I/O on PA1-0
}

void UART_OutChar(unsigned char data){
  while((UART0_FR_R&UART_FR_TXFF) != 0);
  UART0_DR_R = data;
}

void UART_OutUDec(unsigned long n){
// This function uses recursion to convert decimal number
//   of unspecified length as an ASCII string
  if(n >= 10){
    UART_OutUDec(n/10);
    n = n%10;
  }
  UART_OutChar(n+'0'); /* n is between 0 and 9 */
}

void TriggerSignalEnable(){
    SYSCTL_RCGC2_R |= 0x01;
    GPIO_PORTA_PCTL_R &= ~0x00000F00; // 3) regular GPIO
    GPIO_PORTA_AMSEL_R &= ~0x80;      // 4) disable analog function on PA2
    GPIO_PORTA_DIR_R |= 0x80;        // 5) set direction to output
    GPIO_PORTA_AFSEL_R &= ~0x80;      // 6) regular port function
    GPIO_PORTA_DEN_R |= 0x80;         // 7) enable digital port
}

void EnableEdgeModeTimers(){
    SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER0;
    //Enable Input capture pins CCP0 and CCP1
    SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOB + SYSCTL_RCGC2_GPIOF;
    GPIO_PORTB_DEN_R |= 0x80;
    GPIO_PORTB_AFSEL_R |= 0x80;
    GPIO_PORTB_DEN_R |= 0x40;
    GPIO_PORTB_AFSEL_R |= 0x40;
    GPIO_PORTB_PCTL_R |= 0x77000000;

    TIMER0_CTL_R &= ~0x101;
    TIMER0_CFG_R  = 0x00000004;
    TIMER0_TAMR_R = 0x00000017;
    TIMER0_TBMR_R = 0x00000017;
    TIMER0_CTL_R =  (TIMER0_CTL_R &~0x00C) + 0x04; // Timer A is falling
    TIMER0_CTL_R &= ~0xC00;                        // Timer B is rising edge
    TIMER0_IMR_R |= 0x404;
    TIMER0_ICR_R |= 0x404;
    NVIC_PRI4_R = (NVIC_PRI4_R&0x00FFFFFF)|0x40000000;  // Set up NVIC for Timers since be using interrupt driven
    NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFFFF00)|0x00000040;
    NVIC_EN0_R |= 0x0180000;
}

void Timer1AB_Init(void){
  SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER1;   // activate timer1
  TIMER1_CTL_R &= ~0x00000101;             // disable timer1A and timer 1B during setup
  TIMER1_CFG_R =  0x00000004;              // configure for 16-bit timer mode
  TIMER1_TAMR_R = 0x00000001;              // configure for one shot
  TIMER1_TBMR_R = 0x00000001;              // Configure as a one shot timer
  TIMER1_TAILR_R =  100;                  // The "period" of the trigger signal, (could try to make it smaller)
  TIMER1_TBILR_R =  10;                    // So that trigger signal is at least 10usec
  TIMER1_TAPR_R =   1;                     // No Prescaler
  TIMER1_TBPR_R =   1;
  TIMER1_ICR_R =  0x00000101;               // clear timer1A and timer 1B timeout flag
  TIMER1_IMR_R |= 0x00000101;               // arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R & NVIC_PRI5_TIME1AB_MASK)|0x00404000; // priority 2 for both timers
  NVIC_EN0_R |= 0x00600000;                 // enable interrupt 19 in NVIC
  TIMER1_CTL_R |= 0x00000101;               // enable timer1A and timer 1B
  GPIO_PORTA_DATA_R |= 0x80;                // Enable trigger signal
}

void Timer0A_Handler(void){
   if(!risingEdgeMeasured){
      TIMER0_ICR_R |= TIMER_ICR_CAECINT; //acknowledge timer0A capture flag
   }
   else{
      TIMER0_ICR_R = TIMER_ICR_CAECINT; // Acknowledge timer 0A Interrupt
      TIMER0_CTL_R &= ~0x00000100;      //Disable Timer0B Interrupts
      pulseWidth = ((TIMER0_TAR_R&0xFFFF) - (TIMER0_TBR_R&0xFFFF));
      distanceCM = (0.5*pulseWidth*CLOCK_TICK*SPEED_SOUND);
      risingEdgeMeasured = 0;
      measured = 1;
      if(count%5== 0 && measured && distanceCM > 4 && distanceCM < 55){
         UART_OutUDec(distanceCM);
         UART_OutChar('\n');
         count = 0;
         measured = 0;
      }
      overFlowCount = 0;
      TIMER0_CTL_R &= ~0x00000001;      // Input capture until trigger signal enable again.
      TIMER1_CTL_R |= 0x01;             //  "Schedule" start trigger after TIME1A Timeout
   }
}

void Timer0B_Handler(void){
    risingEdgeMeasured = 1;
    TIMER0_ICR_R |= 0x00000400; //acknowledge timer 0B capture flag
}

void Timer1A_Handler(void){
    TIMER1_ICR_R = TIMER_ICR_TATOCINT;
    GPIO_PORTA_DATA_R = 0x80;
    TIMER1_CTL_R |= 0x00000100; // Re enable one shot timer
}
void Timer1B_Handler(void){
    TIMER1_ICR_R = TIMER_ICR_TBTOCINT;
    GPIO_PORTA_DATA_R &= ~0x80;
    TIMER0_TAV_R &= ~0xFFFF;          // Clear the bottom 15:0 bits
    TIMER0_TBV_R &= ~0xFFFF;
    TIMER1_CTL_R &= ~0x00000001;      // Disable timer interrupts right after we trigger
    TIMER0_CTL_R |=  0x00000101;      // enable timer0A and timer 0B
    GPIO_PORTA_DATA_R &= ~0x80;
}
void Timer2A_Handler(void){
    TIMER2_ICR_R  = TIMER_ICR_TATOCINT;
    TIMER2_CTL_R |= 0x00000001;
    ++overFlowCount;
}
