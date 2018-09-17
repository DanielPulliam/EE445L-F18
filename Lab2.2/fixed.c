// filename ******** fixed.c ************** 
// Daniel Pulliam - drp2279
// Brian Dubbert - bpd397
// Created: September 3, 2018
// Last Revision: September 8, 2018
// Lab 1
// TA: Sam Harper

#include <stdio.h>
#include <stdint.h>
#include "fixed.h"
#include "ST7735.h"


// Minimum and Maximum variables for ST7735 LCD
int32_t Xplot_min;
int32_t Xplot_max;
int32_t Yplot_min;
int32_t Yplot_max;

int32_t xMax = 127;
int32_t yMax = 159;
int32_t yOffset = 32;


/****************ST7735_sDecOut2***************
 converts fixed point number to LCD
 format signed 32-bit with resolution 0.01
 range -99.99 to +99.99
 Inputs:  signed 32-bit integer part of fixed-point number
 Outputs: none
 send exactly 6 characters to the LCD 
Parameter LCD display
 12345    " **.**" 
  2345    " 23.45" 
 -8100    "-81.00" 
  -102    " -1.02" 
    31    "  0.31" 
-12345    "-**.**"  
 */ 
void ST7735_sDecOut2(int32_t n){
	int tens, ones, tenths, hundredths;              // Digit placeholder variables
	
	if (n > 9999) {                                  // Check if n is greater than 9999
	  ST7735_OutString(" **.**");                    // Print " **.**" if greater than 9999 and do not execute anything else
		return;
	}
	
	if (n < -9999) {                                 // Check if n is les than -9999
		ST7735_OutString("-**.**");                    // Print "-**.**" if less than -9999 and do not execute anything else
		return;
	}
	
  if (n < 0) {                                     // Check to see if n is negative
	  if (n >= -999) {                               // Check to see if n is less than 4 digits 
		  ST7735_OutString(" ");                       // If n is less than 4 digits, a space is needed in the string
		}
		ST7735_OutString("-");                         // Since n is negative, print a negative sign
		n *= -1;                                       // Since n is negative, make n positive for the later printf statements
	}
	else {                                          
	  if (n <= 999) {                                // Check to see if n is less than 4 digits
		  ST7735_OutString(" ");                       // If n is less than 4 digits, a space is needed in the string
	  }
	  ST7735_OutString(" ");                         // Since n is positive, print a space
	}
			
	tens = (n / 1000);                               // Divide n by 1000 to get tens digit
  ones = ((n / 100) % 10);                         // Divide n by 100 mod 10 to get ones digit
	tenths = ((n / 10) % 10);                        // Divide n by 10 mod 10 to get tenths digit        
	hundredths = (n % 10);                           // n mod 10 to get hundredths digit
	if (tens > 0) {                                  // Check to see if tens digit is greater than 0
		printf("%d", tens);                            // Print tens digit if greater than 0
	}
	if (ones >= 0) {                                 // Check to see if tens digit is greater than or equal to 0
		printf("%d",ones);                             // Print ones digit if greater than or equal to 0
	}
	printf(".%d%d", tenths, hundredths);             // Print decimal place and the rest of the digits
}

/**************ST7735_uBinOut6***************
 unsigned 32-bit binary fixed-point with a resolution of 1/64. 
 The full-scale range is from 0 to 999.99. 
 If the integer part is larger than 63999, it signifies an error. 
 The ST7735_uBinOut6 function takes an unsigned 32-bit integer part 
 of the binary fixed-point number and outputs the fixed-point value on the LCD
 Inputs:  unsigned 32-bit integer part of binary fixed-point number
 Outputs: none
 send exactly 6 characters to the LCD 
Parameter LCD display
     0	  "  0.00"         0/64 = 0
     1	  "  0.01"         1/64 = 0.01
    16    "  0.25"        16/64 = 0.25
    25	  "  0.39"        25/64 = 0.39
   125	  "  1.95"       125/64 = 1.95
   128	  "  2.00"       128/64 = 2.00
  1250	  " 19.53"      1250/64 = 19.53
  7500	  "117.19"      7500/64 = 117.19
 63999	  "999.99"     63999/64 = 999.99
 64000	  "***.**"     64000/64 = ***.**
*/
void ST7735_uBinOut6(uint32_t n){
	int hundreds, tens, ones, tenths, hundredths;     // Digit placeholder variables
  if (n >= 64000) {                                 // If the integer part is larger than 63999,
	  ST7735_OutString("***.**");                     // Signifies an error
		return;                                         // If there is an error, do not execute anything else in function
	}
	n *= 1000;
	n /= 64;                                          // Divide n by 64 to get expected output
	//n >>= 5;                                        // Right shift (division) by 6 -- 2^6 = 64
	if ((n % 10) >= 5) {                              // Three digit number followed by 2 decimal places. Multiply by 100 to see the thousandths place - check to see if rounding is needed
		n += 10;                                        // If rounding is needed, add 10
	}
	hundreds = n / 100000;                            // Divide n by 100000 to get hundreds digit
	tens = (n / 10000) % 10 ;                         // Divide n by 10000 mod 10 to get tens digit
	ones = (n / 1000) % 10;                           // Divide n by 1000 mod 10 to get ones digit
	tenths = (n / 100) % 10;                          // Divide n by 100 mod 10 to get tenths digit
	hundredths = (n / 10) % 10;                       // Divide n by 10 mod 10 to get hundredths digit
	if (hundreds > 0) {                               // Check if hundreds digit is greater than 0
	  printf("%d", hundreds);                         // Print hundreds digit if greater than 0
	}
	else {
		ST7735_OutString(" ");                          // Print a space if not greater than 0
	}
	if ((tens > 0) || (hundreds > 0)) {               // Check if tens digit or if hundreds digit (since tens digit may be 0) is greater than 0
		printf("%d", tens);                             // Print tens digit if greater than 0
	}
	else {
		ST7735_OutString(" ");                          // Print a space if not greater than 0
	}
  printf("%d.%d%d", ones, tenths, hundredths);      // Print the rest of the digits and decimal place
}

/**************ST7735_XYplotInit***************
 Specify the X and Y axes for an x-y scatter plot
 Draw the title and clear the plot area
 Inputs:  title  ASCII string to label the plot, null-termination
          minX   smallest X data value allowed, resolution= 0.001
          maxX   largest X data value allowed, resolution= 0.001
          minY   smallest Y data value allowed, resolution= 0.001
          maxY   largest Y data value allowed, resolution= 0.001
 Outputs: none
 assumes minX < maxX, and miny < maxY
*/
void ST7735_XYplotInit(char *title, int32_t minX, int32_t maxX, int32_t minY, int32_t maxY){
  ST7735_FillScreen(0);           // Set screen to black
  ST7735_SetCursor(0,0);          // Set cursor on LCD to (0,0) - (x,y)
	
	printf("%s", title);            // Print title  
	
	                                // Define bounds
	Xplot_min = minX;               // Set Xplot_min to minX
	Xplot_max = maxX;               // Set Xplot_max to maxX
	Yplot_min = minY;               // Set Yplot_min to minY
	Yplot_max = maxY;               // Set Yplot_max to maxY
}

/**************ST7735_XYplot***************
 Plot an array of (x,y) data
 Inputs:  num    number of data points in the two arrays
          bufX   array of 32-bit fixed-point data, resolution= 0.001
          bufY   array of 32-bit fixed-point data, resolution= 0.001
 Outputs: none
 assumes ST7735_XYplotInit has been previously called
 neglect any points outside the minX maxY minY maxY bounds
*/
void ST7735_XYplot(uint32_t num, int32_t bufX[], int32_t bufY[]){
	int x, y, n, xCoord, yCoord; 
  int ySpace = yMax - yOffset;                            // Determine which y values on LCD will be available to draw to
	int xRange = Xplot_max - Xplot_min;                     // Determine x range 
  int yRange = Yplot_max - Yplot_min;	                    // Determine y range
	
	for (n = 0; n < num; n++) {                             // Cycle through all values of the arrays
		
		xCoord = (bufX[n] - Xplot_min);                       // Determine X coordinate
		yCoord = (Yplot_max - bufY[n]);                       // Determine Y coordinate
		x = ((xMax * xCoord) / (xRange));		                  // Determine x pixel to draw - scaled
		y = (yOffset + ((ySpace * (yCoord)) / (yRange)));     // Determine y pixel to draw - scaled
		
		if ((bufX[n] >= Xplot_min) && (bufX[n] <= Xplot_max) && (bufY[n] >= Yplot_min) && (bufY[n] <= Yplot_max)) {  // Check to see if within bounds
			ST7735_DrawPixel(x, y, ST7735_BLUE);                // Draw pixel if within bounds
		}
	}
}
