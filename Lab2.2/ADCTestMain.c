// filename ******** ADCTestMain.c ************** 
// Daniel Pulliam - drp2279
// Brian Dubbert - bpd397
// Created: September 9, 2018
// Last Revision: September 9, 2018
// Lab 2
// TA: Sam Harper

// ADCTestMain.c
// Runs on TM4C123
// This program periodically samples ADC channel 0 and stores the
// result to a global variable that can be accessed with the JTAG
// debugger and viewed with the variable watch feature.
// Daniel Valvano
// September 5, 2015

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2015

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

// center of X-ohm potentiometer connected to PE3/AIN0
// bottom of X-ohm potentiometer connected to ground
// top of X-ohm potentiometer connected to +3.3V 
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "ADCSWTrigger.h"
#include "tm4c123gh6pm.h"
#include "ST7735.h"
#include "PLL.h"
#include "Timer1.h"

#define PF2             (*((volatile uint32_t *)0x40025010))
#define PF1             (*((volatile uint32_t *)0x40025008))
#define dumpSize        1000
#define no_samples      4096

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

volatile uint32_t ADCvalue;

// PREPARATION (4)
//   DEBUGGING DUMPS FOR THE FIRST 1000 ADC MEASUREMENTS - INDEX, TIMESTAMP DUMP, ADC VALUE DUMP
volatile uint32_t dumpIndex = 0;                     // Index for the two dumps below
volatile static uint32_t currentTimeDump[dumpSize];  // Array (dump) of Timestamps when reading the Timer 1 TIMER1_TAR_R (will return the current time in 12.5ns units)
volatile static uint32_t currentADCDump[dumpSize];   // Array (dump) of ADC Values

// PREPARATION (5)
//   PROCESSING TIME RECORDINGS - LARGEST AND SMALLEST TIME DIFFERENCE
volatile uint32_t largestTime = 0;
volatile uint32_t smallestTime = 0xFFFFFFFF;
volatile static uint32_t timeDifference[dumpSize];
volatile uint32_t timeJitter;

// PREPARATION (6)
//   PROCESSING DATA RECORDINGS 
volatile uint32_t largestADCValue = 0;
volatile uint32_t smallestADCValue = 0xFFFFFFFF;
volatile static uint32_t no_occurrences[no_samples]; // each index within this arra will denote the number of occurrences of each ADC value
volatile uint32_t min, max = 0;


// This debug function initializes Timer0A to request interrupts
// at a 100 Hz frequency.  It is similar to FreqMeasure.c.
void Timer0A_Init100HzInt(void){
  volatile uint32_t delay;
  DisableInterrupts();
  // **** general initialization ****
  SYSCTL_RCGCTIMER_R |= 0x01;      // activate timer0
  delay = SYSCTL_RCGCTIMER_R;      // allow time to finish activating
  TIMER0_CTL_R &= ~TIMER_CTL_TAEN; // disable timer0A during setup
  TIMER0_CFG_R = 0;                // configure for 32-bit timer mode
  // **** timer0A initialization ****
                                   // configure for periodic mode
  TIMER0_TAMR_R = TIMER_TAMR_TAMR_PERIOD;
  TIMER0_TAILR_R = 799999;         // start value for 100 Hz interrupts
  TIMER0_IMR_R |= TIMER_IMR_TATOIM;// enable timeout (rollover) interrupt
  TIMER0_ICR_R = TIMER_ICR_TATOCINT;// clear timer0A timeout flag
  TIMER0_CTL_R |= TIMER_CTL_TAEN;  // enable timer0A 32-b, periodic, interrupts
  // **** interrupt initialization ****
                                   // Timer0A=priority 2
  NVIC_PRI4_R = (NVIC_PRI4_R&0x00FFFFFF)|0x40000000; // top 3 bits
  NVIC_EN0_R = 1<<19;              // enable interrupt 19 in NVIC
}

void Timer0A_Handler(void){
  TIMER0_ICR_R = TIMER_ICR_TATOCINT;                 // acknowledge timer0A timeout
  PF2 ^= 0x04;                                       // profile
  PF2 ^= 0x04;                                       // profile
  ADCvalue = ADC0_InSeq3();
  PF2 ^= 0x04;                                       // profile
	
	if (dumpIndex < dumpSize) {                        // Check to see if the current dump index is less than the dump size (1000)  
		currentTimeDump[dumpIndex] = TIMER1_TAR_R;       // Read current time - Set current dump index value of currentTimeDump to TIMER1_TAR_R (returns the current time)
		currentADCDump[dumpIndex] = ADCvalue;            // Read ADC value - Set current dump index value of currentADCDump to ADCvalue
		dumpIndex += 1;                                  // Increase dump index by 1
	}
}

void DiagonalLine(int16_t rise, int16_t run, int16_t x, int16_t x1, int16_t y1, uint16_t color){
	int16_t y = (((rise * (x - x1)) / (run)) + y1);         // Linear Equatio for a diagonal line
	ST7735_DrawPixel(x, y, color);                          // Draw Diagonal Line using x values from for loop, and y value from linear equation
}

//************* ST7735_DrawLine********************************************
// Draws one line on the ST7735 color LCD
// Inputs: (x1,y1) is the start point
// 				 (x2,y2) is the end point
// x1,x2 are horizontal positions, columns from the left edge
// 					must be less than 128
// 					0 is on the left, 126 is near the right
// y1,y2 are vertical positions, rows from the top edge
// 					must be less than 160
// 					159 is near the wires, 0 is the side opposite the wires
// color 16-bit color, which can be produced by ST7735_Color565()
// Output: none
void ST7735_DrawLine (int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  int16_t height = y2 - y1;                                   // Calculate difference between first y coordinate and second y coordinate to get height
	int16_t width = x2 - x1;                                    // Calculate difference between first x coordinate and second x coordinate to get width
	int16_t rise, run = 0;                                      // Used for Diagonal Lines - Linear Equation
  int16_t x = 0;                                              // Will be used as part of a function (f(x))

	if (y2 == y1) {                                             // Determine if the line needed to be drawn is a horizontal line
	  if (x2 > x1) {                                            // Determine if the line needed to be drawn is a horizontal line pointing towards the right
		  ST7735_DrawFastHLine(x1, y1, width, color);             // Draw a Horizontal Line at y1 from x1 to x2, with width of x2-x1
			return;                                                 // After drawing horizontal line, return from function call
		}
		
		else {                                                    // Line needed to be drawn is a horizontal line pointing towards the left
			ST7735_DrawFastHLine(x2, y2, (width * (-1)), color);    // Draw a Horizontal Line at y2 from x2 to x1, with width of x1-x2
		  return;                                                 // After drawing horizontal line, return from function call
		} 
	}
	
	else if (x2 == x1) {                                        // Determine if the line needed to be drawn is a vertical line
		if (y2 > y1) {                                            // Determine if the line needed to be drawn is a vertical line pointing upwards
			ST7735_DrawFastVLine(x1, y1, height, color);            // Draw a Vertical Line at x1 from y1 to y2, with height of y2-y1
			return;                                                 // After drawing vertical line, return from function call
		}
		
		else {                                                    // Line needed to be drawn is a vertical line pointing downwards
			ST7735_DrawFastVLine(x2, y2, (height * (-1)), color);   // Draw a Vertical Line at x2 from y2 to y1, with height of y1-y2
			return;                                                 // After drawing vertical line, return from function call
		} 
	}
	
  rise = height;                                              // Set rise to height for Linear Equation
	run = width;                                                // Set run to width for Linear Equation
	
	if (x2 < x1) {                                           		// Determine if the line needed to be drawn is a diagonal line pointing towards the left
		for (x = x2; x < x1; x++) {                               // Cycle through all values of x on a diagonal line pointing towards the left
      DiagonalLine(rise, run, x, x1, y1, color);              // Draw Diagonal Line Function call
		}
		return;                                                   // After drawing diagonal line, return from function call
	}
	
	else {                                                      // Determine if the line needed to be drawn is a diagonal line pointing towards the right
		for (x = x1; x < x2; x++) {                               // Cycle through all values of x on a diagonal line pointing towards the right
      DiagonalLine(rise, run, x, x1, y1, color);              // Draw Diagonal Line Function call
		}
		return;                                                   // After drawing diagonal line, return from function call
	}
}

void DelayWait(uint32_t time){;
  while(time){  
	  time--;
  }
}

void Lab2_Demo_DrawLines(void){
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
  ST7735_OutString("Hello!\rEE445L Playground\r\r");
	DelayWait(40000000);
	ST7735_OutString("Drawing Lines...");
  DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
  ST7735_OutString("Draw line from\r(0, 0) to (127, 0)\rHorizontal");
  ST7735_DrawLine(0, 0, 127, 0, ST7735_BLUE);
	DelayWait(40000000);

	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
  ST7735_OutString("Draw line from\r(0, 0) to (0, 159)\rVertical");
	ST7735_DrawLine(0, 0, 0, 159, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
  ST7735_OutString("Draw line from\r(0, 0) to (127, 159)\rDiagonal");
	ST7735_DrawLine(0, 0, 127, 159, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
  ST7735_OutString("Draw line from\r(127,0) to (0, 159)\rDiagonal");
	ST7735_DrawLine(127, 0, 0, 159, ST7735_BLUE);
	DelayWait(40000000);

	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
  ST7735_OutString("Draw line from\r(63, 80)-(127, 159)\rMidpoint - Bot right");
	ST7735_DrawLine(63, 80, 127, 159, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(95, 159)");
	ST7735_DrawLine(63, 80, 97, 159, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(95, 0)");
	ST7735_DrawLine(63, 80, 97, 0, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(32,159)");
	ST7735_DrawLine(63, 80, 32, 159, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(32, 0)");
	ST7735_DrawLine(63, 80, 32, 0, ST7735_BLUE);
	DelayWait(40000000);
	
  ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(0, 40)");
	ST7735_DrawLine(63, 80, 0, 40, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(0, 120)");
	ST7735_DrawLine(63, 80, 0, 120, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(127, 40)");
	ST7735_DrawLine(63, 80, 127, 40, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(127, 120)");
	ST7735_DrawLine(63, 80, 127, 120, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(95, 120)");
	ST7735_DrawLine(63, 80, 95, 120, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(32, 120)");
	ST7735_DrawLine(63, 80, 32, 120, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(95, 40)");
	ST7735_DrawLine(63, 80, 95, 40, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
	ST7735_OutString("Draw line from\r(63, 80)-(32, 40)");
	ST7735_DrawLine(63, 80, 32, 40, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	
  ST7735_OutString("Draw line from\r(63, 80) to (32, 80)");
	ST7735_DrawLine(63, 80, 32, 80, ST7735_BLUE);
	DelayWait(40000000);
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
	ST7735_OutString("Lab 2 Complete!");
	DelayWait(40000000);
}

int main(void){
	int timeDifferencesIndex, dataRecordingsIndex;                   // Loop variables 	                
	int no_TimeDifferences = dumpSize - 1;                           // 999 Time Differences
	int no_DataRecordings = dumpSize;                                // 1000 Data Recordings
	
	DisableInterrupts();                                             // Disable Interrupts
  PLL_Init(Bus80MHz);                                              // 80 MHz
  SYSCTL_RCGCGPIO_R |= 0x20;                                       // activate port F
  ADC0_InitSWTriggerSeq3_Ch9();                                    // allow time to finish activating
	Timer0A_Init100HzInt();                                          // set up Timer0A for 100 Hz interrupts
  GPIO_PORTF_DIR_R |= 0x06;                                        // make PF2, PF1 out (built-in LED)
  GPIO_PORTF_AFSEL_R &= ~0x06;                                     // disable alt funct on PF2, PF1
  GPIO_PORTF_DEN_R |= 0x06;                                        // enable digital I/O on PF2, PF1
                                                                   // configure PF2 as GPIO
  GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFFF00F)+0x00000000;
  GPIO_PORTF_AMSEL_R = 0;                                          // disable analog functionality on PF
  PF2 = 0;                                                         // turn off LED
	Timer1_Init();                                                   // set up Timer1
	ST7735_InitR(INITR_REDTAB);                                      // ST7735 setup and initialization
	
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
  ST7735_OutString("EE 445L Lab 2\r\r");
	DelayWait(25000000);
	ST7735_OutString("Daniel Pulliam\r- drp2279\r\rBrian Dubbert\r- bpd397\r\r");
	DelayWait(25000000);
	ST7735_OutString("Begin Sampling...");
	
  EnableInterrupts();                                              // Enable Interrupts
  
	/*
	while(1){
    PF1 ^= 0x02;                                                   // toggles when running in main
  }
	*/
	
// DATA COLLECTION PHASE
  while (dumpIndex < dumpSize) {
		//PF1 ^= 0x02;
		PF1 = (PF1 * 12345678) / 1234567 + 0x02;                       // this line causes jitter
	}
// END OF DATA COLLECTION PHASE
	
	DisableInterrupts();                                             // Disable Interrupts to ensure the end of the data collection phase
  //smallestTime = currentTimeDump[0];	

// PROCESS TIME AND DATA RECORDINGS
//   OCCURS AFTER THE DUMPS ARE FULL AND NOT DURING THE DATA COLLECTION PHASE
// FIRST: PROCESS TIME RECORDINGS
//   Calculate 999 time differences and then determine time jitter, which is the diference between the largest and smallest time difference
//   The objective is to start the ADC every 10 ms, so any time difference other than 10 ms is an error.
  for (timeDifferencesIndex = 0; timeDifferencesIndex < no_TimeDifferences; timeDifferencesIndex++) {
		// To measure elapsed time, we read TIMER1_TAR_R twice and subtract the second measurement [timeDifferencesIndex+1] from the first [timeDifferencesIndex]
		timeDifference[timeDifferencesIndex] = abs(currentTimeDump[timeDifferencesIndex] - currentTimeDump[timeDifferencesIndex + 1]); // Measures elapsed time
		if (timeDifference[timeDifferencesIndex] < smallestTime) {
			smallestTime = timeDifference[timeDifferencesIndex];
		}
		
		else if (timeDifference[timeDifferencesIndex] > largestTime) {
		  largestTime = timeDifference[timeDifferencesIndex];
		}
	}
	
	// After calculating 999 time differences, determine time jitter
	// Time Jitter is the difference between the largest and smallest time difference
	// Hence, largestTime - smallestTime = Time Jitter
	timeJitter = ((abs(largestTime - smallestTime)) / 10000); // If this value is larger than 10 ms, this signifies an error
	
	/*
	if (timeJitter > 10ms) {// calculate value for 10 ms
		printf("Error - Time Jitter is Greater than 10 ms.");
	}
	*/
	
// SECOND: PROCESS DATA RECORDINGS
//   Assume the analog input is fixed, so any variations in data will be caused by noise. 
//   Create a Probability Mass Function of the measured data.
//   The shape of this curve descirbes the noise process.
//   The x-axis has the ADC value and the y-axis is the number of occurrences of that ADC value.
  for(dataRecordingsIndex = 0; dataRecordingsIndex < no_DataRecordings; dataRecordingsIndex++) {
	  no_occurrences[currentADCDump[dataRecordingsIndex]] += 1; // Increment the number of occurrences of each ADC value collected prior to now
		                                                          // Essentially the Y-axis of a PMF histogram, with X-axis being the ADC values collected  
    if (currentADCDump[dataRecordingsIndex] < smallestADCValue) {
		  smallestADCValue = currentADCDump[dataRecordingsIndex];
		}
		
		if (currentADCDump[dataRecordingsIndex] > largestADCValue) {
		  largestADCValue = currentADCDump[dataRecordingsIndex];
	  }
		
		if(no_occurrences[currentADCDump[dataRecordingsIndex]] > max){
      max = no_occurrences[currentADCDump[dataRecordingsIndex]];
		}
		
		else if(no_occurrences[currentADCDump[dataRecordingsIndex]] < min){
			min = no_occurrences[currentADCDump[dataRecordingsIndex]];
		}
	}
	ST7735_FillScreen(ST7735_BLACK); 
	ST7735_SetCursor(0,0);
  printf("EE 445L Lab 2\rTime Jitter: %d ms", timeJitter);
  ST7735_PlotClear(min,max);
	for(int i=0; i<1000; i++){
	  ST7735_PlotLine(no_occurrences[i]);
		//ST7735_PlotBar(no_occurrences[i]);
	}
	DelayWait(150000000);
	while(1){
    Lab2_Demo_DrawLines();
  }
}
