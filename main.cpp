#include "GrabBitmaps.h" 

PBITMAPINFO pBitmap = NULL;
ULONG BitmapSize = 0; 
  
int main(void)
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);   
	
	CGrabBitmap gb;
	gb.GrabBitmap(&pBitmap, &BitmapSize);
	gb.SaveData(); 

	CoUninitialize();
	return 0;
} 
