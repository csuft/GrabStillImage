#ifndef GRABBITMAPS_H
#define GRABBITMAPS_H

#include <windows.h>
#include <streams.h>
#include <stdio.h>
#include <atlbase.h>
#include <dshow.h>
#include <qedit.h>
#include <strsafe.h>

class CGrabBitmap
{
public:
	CGrabBitmap();
	virtual ~CGrabBitmap();
	int GrabBitmap(PBITMAPINFO *Bitmap, ULONG *BitmapLength); 
	void SaveData();
	
protected:
	HRESULT GetPin(IBaseFilter * pFilter, PIN_DIRECTION dirrequired,  int iNum, IPin **ppPin);
	IPin *  GetInPin ( IBaseFilter *pFilter, int Num );
	IPin *  GetOutPin( IBaseFilter *pFilter, int Num );
	void GetDefaultCapDevice( IBaseFilter ** ppCap);
	static ULONG CalcBitmapInfoSize(const BITMAPINFOHEADER &bmiHeader);

private:

};

#endif