#ifndef GRABBITMAPS_H
#define GRABBITMAPS_H

#include <windows.h>
#include <streams.h>
#include <stdio.h>
#include <atlbase.h>
#include <dshow.h>
#include <qedit.h>
#include <strsafe.h>
#include <string>

class CGrabBitmap
{
public:
	CGrabBitmap();
	virtual ~CGrabBitmap();
	int GrabBitmap();  
	
protected:
	HRESULT GetPin(IBaseFilter * pFilter, PIN_DIRECTION dirrequired,  int iNum, IPin **ppPin);
	IPin *  GetInPin ( IBaseFilter *pFilter, int Num );
	IPin *  GetOutPin( IBaseFilter *pFilter, int Num );
	void GetDefaultCapDevice( IBaseFilter ** ppCap);

private:
	void ReadOffset();
	void RGB2RGBA(unsigned char* rgba, unsigned char* rgb, int imageSize);
	void FlipImageVertically(unsigned char *pixels, const size_t width, const size_t height, const size_t bytes_per_pixel);
private:
	std::string mOffset;
};

#endif