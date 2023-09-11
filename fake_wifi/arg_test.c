#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef unsigned char bool;
typedef signed char int8;
typedef unsigned char uint8;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed int int32;
typedef unsigned int uint32;
typedef signed long long int64;
typedef unsigned long long uint64;


int main( int argc, char *argv[] )  
{
   if( argc == 3 )
   {
      printf("The argument argv[0] is %s\n", argv[0]);
      printf("The argument argv[1] is %s\n", argv[1]);
      printf("The argument argv[2] is %d\n", atoi(argv[2]));
   }
   else if( argc > 3 )
   {
      printf("Too many arguments supplied.\n");
   }
   else
   {
      printf("One argument expected.\n");
   }
}