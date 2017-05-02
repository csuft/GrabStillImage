#include "GrabBitmaps.h"  
  
int main(void)
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);   
	
	CGrabBitmap gb;
	gb.GrabBitmap(); 

	CoUninitialize();

	return 0;
} 
