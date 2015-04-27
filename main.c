/*
 * BrightBike - a simple led strip controller
 *
 * main.c
 * last edited 27/04/2015
 */

#include <stdint.h>
#include "msp430g2452.h"

#define OFF   0
#define FLASH 1
#define POP   2
#define FULL  3

//stores current mode pattern
uint8_t mode = OFF;

void main(void) {
   // Stop watchdog timer
   WDTCTL = WDTPW | WDTHOLD;

   /*Initialise clocks.
    *MCLK & SMCLK sourced from DCO.
    *ACLK sourced from internal VLO.
    *MCLK = 1MHz, SMCLK = 1MHz, ACLK ~ 12kHz.
    */
   BCSCTL1 = CALBC1_1MHZ; //Set DCO to 1MHz.
   DCOCTL = CALDCO_1MHZ;
   BCSCTL3 = LFXT1S_2; //ACLK set to VLO

   /*Initialise Pins
    */
   //power savings..
   P1DIR = 0xFF;
   P2DIR = 0xFF;
   P1OUT = 0xFF;
   P2OUT = 0xFF;

   P1DIR |= BIT2|BIT7; // Set P1.2 , P1.7 to output direction
   P1DIR &= ~BIT3; //Set P1.3 to input.
   P1OUT &= ~(BIT2|BIT7); //Set P1.2, P1.7 to LOW.
   P1SEL |= BIT2; //Enable timer output on P1.2
   P1OUT |= BIT3; // Enable P1.3 internal pull-up
   P1REN |= BIT3;
   P1IE |= BIT3; // P1.3 interrupt enabled
   P1IES |= BIT1; // P1.3 Hi/lo edge
   P1IFG &= ~BIT3; // P1.3 IFG cleared
   /*Intialise ADC for battery voltage sensing
    */
   ADC10CTL0 = SREF_1|ADC10SR|REFON|ADC10SHT_2|ADC10ON; //Internal 1.5v ref.
   ADC10CTL1 = ADC10SSEL_3|INCH_11; //SMCLK, input (Vcc-Vss)/2
   __delay_cycles(1000); //Wait for ADC ref to settle.
   ADC10CTL0 |= ENC + ADC10SC; //Start sampling & conversion.
   /*Initialise TimerA
    */
   TACCR0 = 0x00FF;    //Timer Period ~10ms / ~100Hz
   TACCTL1 = OUTMOD_7; //Set-Reset mode.
   TACCR1 = 0x005F;    //Set PWM pulse width to max.
   TACTL = TASSEL_2 | MC_1 |TAIE;

   //Infinte loop to update CCR1 value.
   uint16_t i = 0;
   while(1){
      _BIS_SR(LPM1_bits + GIE); //Enter LPM1 with interrupts.
      //Measure battery voltage
      while(ADC10CTL1 & BUSY); //Wait for conversion.
      if(ADC10MEM <= 682){ //Less than 2 volts
         P1OUT &= ~BIT7;   //Switch off indicator led
      }
      ADC10CTL0&=~(ENC+ADC10ON); //return to wait for ENC state.

      switch(mode){
      case FULL:
         P1OUT|=BIT7;
         TACCR1 = 0x00FF;
         break;
      case POP:
         P1OUT|=BIT7;
         //replace with nicer code here...
         if(i == 1000){
            TACCR1 = 0x0010;
         } else if(i == 2000){
            TACCR1 = 0x0025;
         } else if(i == 3000){
            TACCR1 = 0x0010;
         } else if(i == 4000){
            TACCR1 = 0x0025;
         } else if(i == 5000){
            TACCR1 = 0x0010;
         } else if(i >= 6000){
            TACCR1 = 0x00FF;
            i = 0;
         }
         i++;
         break;
      case FLASH:
         P1OUT|=BIT7;
         if(i == 1000){
            TACCR1 = 0x0010;
         } else if(i >= 2000){
            TACCR1 = 0x00FF;
            i = 0;
         }
         i++;
         break;
      case OFF:
         P1OUT &= ~BIT7;
         TACCR1 = 0;
         break;
      }
   }
}


// Port 1 interrupt service routine
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void){
   if((P1IFG && BIT3) == 1){
      //Loop through modes when button at P1.3 pressed.
      //What Bounce?
      if(mode == OFF){
         mode = FULL;
      } else{
         mode--;
      }

      P1IFG &= ~BIT3; //Clear IFG
   }
}

// Timer 1 interrupt service routine
#pragma vector=TIMER0_A1_VECTOR
__interrupt void  Timer_1(void){
   TACTL &= ~TAIFG;
   ADC10CTL0 |= ENC + ADC10SC; //Start sampling & conversion.
   LPM1_EXIT; //Return to active mode to service CCR1.
}
