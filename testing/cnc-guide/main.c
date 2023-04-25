#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#define MAX 100
#define MIN 10
#define TRAVEL MAX-MIN



//==== VARIABLES ====
bool direction = true;
uint16_t posNow = 10;



//==== FUNCTIONS ====

//print position
//print line that illustrates the position pos between 0 and MAX 
//e.g. "|----<=>--------|"
void printPos(int pos){
    int width = 50;
    printf("(%d)|", pos);
    for (int i = 0; i<(pos) * width/MAX; i++) printf("-");
    printf("<=>");
    for (int i = 0; i<(MAX-pos) * width/MAX; i++) printf("-");
    printf("|\n");
    return;
}


//move "virtual axis" to absolute coordinate in mm
void moveToAbs(int d){
    int posOld = posNow;
    printf ("moving from %d %s to %d (%s)\n", posNow, direction ? "=>" : "<=", d, direction ? "RIGHT" : "LEFT");
    posNow = d;
    //illustrate movement (print position for each coordinate change)
    for (int i=posOld; i!=posNow; i+=posNow>posOld?1:-1) printPos(i);
    return;
}


//travel back and forth between MIN and MAX coordinate for a certain distance 
//negative values are processed reversed
void travel(int length){
    int d, remaining;
    d = abs(length);
    if(length < 0) direction = !direction; //invert direction in reverse mode

    while (d != 0){
        //--- currently moving right ---
        if (direction == true){         //currently moving right
            remaining = MAX - posNow;   //calc remaining distance fom current position to limit
            if (d > remaining){         //new distance will exceed limit
                moveToAbs (MAX);        //move to limit
                direction = false;      //change current direction for next iteration
                d = d - remaining;      //decrease target length by already traveled distance
                printf(" --- changed direction (L) --- \n ");
            }
            else {                      //target distance does not reach the limit
                moveToAbs (posNow + d); //move by (remaining) distance to reach target length
                d = 0;                  //finished, reset target length (could as well exit loop/break)
            }
        }

        //--- currently moving left ---
        else {
            remaining = posNow - MIN;
            if (d > remaining){
                moveToAbs (MIN);
                direction = true;
                d = d - remaining;
                printf(" --- changed direction (R) --- \n ");
            }
            else {
                moveToAbs (posNow - d); //when moving left the coordinate has to be decreased
                d = 0;
            }
        }
    }

    if(length < 0) direction = !direction; //undo inversion of direction after reverse mode is finished
    return;
}



//==== MAIN ====
int main (void)
{
    int input;
    printf("**cable-length-cutter testing cnc-guide**\n");
    while(1){
        printf("enter mm to travel:");
        scanf("%d", &input);
        travel(input);
        printf("\n");
    }
    return 0;
}

