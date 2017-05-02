//------------------------------------------------------------------------------
// File: GrabBitmaps.cpp
//
// Desc: DirectShow class
//       This class will connect to a video capture device,
//       create a filter graph with a sample grabber filter,
//       and read a frame out of it to a memory location.
//
// Author: Audrey J.W. Mbogho walegwa@yahoo.com
//------------------------------------------------------------------------------
#pragma comment(lib, "legacy_stdio_definitions.lib")
#include "GrabBitmaps.h"
#include "BlenderWrapper.h"
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>  
#include <fstream>
#include <opencv2/opencv.hpp>

using namespace cv;

// Globals
typedef struct _callbackinfo 
{
    double dblSampleTime;
    long lBufferSize;
    BYTE *pBuffer;
    BITMAPINFOHEADER bih;
	DWORD biSize;
} CALLBACKINFO;

CALLBACKINFO cbInfo={0};

// Implementation of CSampleGrabberCB object
//
// Note: this object is a SEMI-COM object, and can only be created statically.

class CSampleGrabberCB : public ISampleGrabberCB 
{

public:

    // These will get set by the main thread below. We need to
    // know this in order to write out the bmp
    long Width;
    long Height;

    // Fake out any COM ref counting
    //
    STDMETHODIMP_(ULONG) AddRef() { return 2; }
    STDMETHODIMP_(ULONG) Release() { return 1; }

    // Fake out any COM QI'ing
    //
    STDMETHODIMP QueryInterface(REFIID riid, void ** ppv)
    {
        CheckPointer(ppv,E_POINTER);
        
        if( riid == IID_ISampleGrabberCB || riid == IID_IUnknown ) 
        {
            *ppv = (void *) static_cast<ISampleGrabberCB*> ( this );
            return NOERROR;
        }    

        return E_NOINTERFACE;
    }


    // We don't implement this one
    //
    STDMETHODIMP SampleCB( double SampleTime, IMediaSample * pSample )
    {
        return 0;
    }


    // The sample grabber is calling us back on its deliver thread.
    // This is NOT the main app thread!
    //
	STDMETHODIMP BufferCB( double dblSampleTime, BYTE * pBuffer, long lBufferSize )
    {
        if (!pBuffer)
            return E_POINTER;

        if( cbInfo.lBufferSize < lBufferSize )
        {
            delete [] cbInfo.pBuffer;
            cbInfo.pBuffer = NULL;
            cbInfo.lBufferSize = 0;
        }

        // Since we can't access Windows API functions in this callback, just
        // copy the bitmap data to a global structure for later reference.
        cbInfo.dblSampleTime = dblSampleTime;

        // If we haven't yet allocated the data buffer, do it now.
        // Just allocate what we need to store the new bitmap.
        if (!cbInfo.pBuffer)
        {
            cbInfo.pBuffer = new BYTE[lBufferSize];
            cbInfo.lBufferSize = lBufferSize;
        }

        if( !cbInfo.pBuffer )
        {
            cbInfo.lBufferSize = 0;
            return E_OUTOFMEMORY;
        }

        // Copy the bitmap data into our global buffer
        memcpy(cbInfo.pBuffer, pBuffer, lBufferSize);

        return 0;
    }   
};

// This semi-COM object will receive sample callbacks for us
//
CSampleGrabberCB CB;

// CGrabBitmap class constructor
//
CGrabBitmap::CGrabBitmap()
{
}

// CGrabBitmap class destructor
//
CGrabBitmap::~CGrabBitmap()
{
}

int CGrabBitmap::GrabBitmap()
{
    USES_CONVERSION;
    CComPtr< ISampleGrabber > pGrabber;
    CComPtr< IBaseFilter >    pSource;
    CComPtr< IGraphBuilder >  pGraph;
    CComPtr< IVideoWindow >   pVideoWindow;
    HRESULT hr;

	ReadOffset();
    // Create the sample grabber
    //
    pGrabber.CoCreateInstance(CLSID_SampleGrabber);
    if( !pGrabber )
    {
        return 0;
    }
    CComQIPtr< IBaseFilter, &IID_IBaseFilter > pGrabberBase( pGrabber );

    // Get whatever capture device exists
    //
	GetDefaultCapDevice(&pSource);
	if( !pSource )
    {
        return 0;
    }

    // Create the graph
    //
    pGraph.CoCreateInstance( CLSID_FilterGraph );
    if( !pGraph )
    {
        return 0;
    }

    // Put them in the graph
    //
    hr = pGraph->AddFilter( pSource, L"Source" );
    hr = pGraph->AddFilter( pGrabberBase, L"Grabber" );

    // Tell the grabber to grab 24-bit video. Must do this
    // before connecting it
    //
    CMediaType GrabType;
    GrabType.SetType( &MEDIATYPE_Video );
    GrabType.SetSubtype( &MEDIASUBTYPE_ARGB32 );
    hr = pGrabber->SetMediaType( &GrabType );

    // Get the output pin and the input pin
    //
    CComPtr< IPin > pSourcePin;
    CComPtr< IPin > pGrabPin;

    pSourcePin = GetOutPin( pSource, 0 );
    pGrabPin   = GetInPin( pGrabberBase, 0 );

	// set output format
	CComPtr<IAMStreamConfig> pCfg = 0;;
	hr = pSourcePin->QueryInterface(IID_IAMStreamConfig, (void **)&pCfg);
	int   iCount = 0, iSize = 0;
	hr = pCfg->GetNumberOfCapabilities(&iCount, &iSize);
	if (iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS))
	{
		// Use the video capabilities structure.  
		for (int iFormat = 0; iFormat < iCount; iFormat++)
		{
			VIDEO_STREAM_CONFIG_CAPS   scc;
			VIDEOINFOHEADER*   pVih;
			BITMAPINFOHEADER*   pBih = NULL;
			AM_MEDIA_TYPE   *pmtConfig;
			hr = pCfg->GetStreamCaps(iFormat, &pmtConfig, (BYTE*)&scc);
			if (SUCCEEDED(hr))
			{
				/* Examine the format,and possibly use it. */
				pVih = (VIDEOINFOHEADER*)pmtConfig->pbFormat;
				pBih = &pVih->bmiHeader;
				int width = pBih->biWidth;
				int height = pBih->biHeight;
				printf("Current Width: %d, Height: %d\n", width, height);
				if (width == 3008 && height == 1504)
				{
					pVih->AvgTimePerFrame = 10000000 / 15;
					hr = pCfg->SetFormat(pmtConfig);
					FreeMediaType(*pmtConfig);
					break;
				}
				FreeMediaType(*pmtConfig);
			}
		}
	}

    // ... and connect them
    //
    hr = pGraph->Connect( pSourcePin, pGrabPin );
    if( FAILED( hr ) )
    {
        return 0;
    }

    // Ask for the connection media type so we know its size
    //
    AM_MEDIA_TYPE mt;
    hr = pGrabber->GetConnectedMediaType( &mt );

    VIDEOINFOHEADER * vih = (VIDEOINFOHEADER*) mt.pbFormat;
    CB.Width  = vih->bmiHeader.biWidth;
    CB.Height = vih->bmiHeader.biHeight;
    FreeMediaType( mt );

	// Write the bitmap format
    //
      
    memset( &(cbInfo.bih), 0, sizeof( cbInfo.bih ) );
    cbInfo.bih.biSize = sizeof( cbInfo.bih );
    cbInfo.bih.biWidth = CB.Width;
    cbInfo.bih.biHeight = CB.Height;
    cbInfo.bih.biPlanes = 1;
    cbInfo.bih.biBitCount = 32;

	printf("Width: %d, Height: %d\n", CB.Width, CB.Height);
    // Render the grabber output pin (to a video renderer)
    //
    CComPtr <IPin> pGrabOutPin = GetOutPin( pGrabberBase, 0 );
    hr = pGraph->Render( pGrabOutPin );
    if( FAILED( hr ) )
    {
        return 0;
    }

    // Don't buffer the samples as they pass through
    //
    hr = pGrabber->SetBufferSamples( FALSE );

    // Only grab one at a time, stop stream after
    // grabbing one sample
    //
    hr = pGrabber->SetOneShot( TRUE );

    // Set the callback, so we can grab the one sample
    //
    hr = pGrabber->SetCallback( &CB, 1 );

    // Query the graph for the IVideoWindow interface and use it to
    // disable AutoShow.  This will prevent the ActiveMovie window from
    // being displayed while we grab bitmaps from the running movie.
    CComQIPtr< IVideoWindow, &IID_IVideoWindow > pWindow = pGraph;
    if (pWindow)
    {
        hr = pWindow->put_AutoShow(OAFALSE);
    }

    // activate the threads
    CComQIPtr< IMediaControl, &IID_IMediaControl > pControl( pGraph );
    hr = pControl->Run( );

	// wait for the graph to settle
    CComQIPtr< IMediaEvent, &IID_IMediaEvent > pEvent( pGraph );
    long EvCode = 0;

    hr = pEvent->WaitForCompletion( INFINITE, &EvCode );
        
	if (cbInfo.pBuffer == nullptr || cbInfo.lBufferSize == 0)
	{
		printf("Failed to read sample frame.\nMaybe the camera is busy right now...\n");
		return cbInfo.lBufferSize;
	}
	CBlenderWrapper* m_blender = new CBlenderWrapper;
	m_blender->capabilityAssessment();
	m_blender->getSingleInstance(CBlenderWrapper::FOUR_CHANNELS);
	m_blender->initializeDevice();

	Mat outputBuffer(cbInfo.bih.biHeight, cbInfo.bih.biWidth, CV_8UC4);
	Mat ResultImage(cbInfo.bih.biHeight, cbInfo.bih.biWidth, CV_8UC4);

	BlenderParams params;
	params.input_width = cbInfo.bih.biWidth;
	params.input_height = cbInfo.bih.biHeight;
	params.output_width = cbInfo.bih.biWidth;
	params.output_height = cbInfo.bih.biHeight;
	params.input_data = cbInfo.pBuffer;
	params.output_data = ResultImage.data;
	params.offset = mOffset; 

	FlipImageVertically(cbInfo.pBuffer, params.input_width, params.input_height, 4);
	m_blender->runImageBlender(params, CBlenderWrapper::PANORAMIC_BLENDER);
	
	imwrite("Insta360_Air.jpg", ResultImage);

    return cbInfo.lBufferSize;
}

/**
* pixels_buffer - Pixels buffer to be operated
* width - Image width
* height - Image height
* bytes_per_pixel - Number of image components, ie: 3 for rgb, 4 rgba, etc...
**/
void CGrabBitmap::FlipImageVertically(unsigned char *pixels, const size_t width, const size_t height, const size_t bytes_per_pixel)
{
	const size_t stride = width * bytes_per_pixel;
	unsigned char *row = (unsigned char*)malloc(stride);
	unsigned char *low = pixels;
	unsigned char *high = &pixels[(height - 1) * stride];

	for (; low < high; low += stride, high -= stride) {
		memcpy(row, low, stride);
		memcpy(low, high, stride);
		memcpy(high, row, stride);
	}
	free(row);
}

void CGrabBitmap::RGB2RGBA(unsigned char* rgba, unsigned char* rgb, int imageSize)
{
	if (rgba == nullptr || rgb == nullptr || imageSize <= 0)
	{
		return;
	}
	int rgbIndex = 0;
	int rgbaIndex = 0;

	while (rgbIndex < imageSize) {
		rgba[rgbaIndex] = rgb[rgbIndex];
		rgba[rgbaIndex + 1] = rgb[rgbIndex + 1];
		rgba[rgbaIndex + 2] = rgb[rgbIndex + 2];
		rgba[rgbaIndex + 3] = 255;
		rgbIndex += 3;
		rgbaIndex += 4;
	}
}

void CGrabBitmap::ReadOffset()
{
	char buffer[128] = { '\0' };
	char* home = getenv("HOMEDRIVE");
	if (home != NULL)
	{
		std::string fullPath = std::string(home);
		home = getenv("HOMEPATH");
		if (home != NULL)
		{
			fullPath = fullPath + home;
			fullPath = fullPath + "\\AppData\\Local\\insta360\\USBCamera\\uvcoffset";   

			std::ifstream offset(fullPath);
			if (!offset)
			{
				printf("Can't open offset file, use default offset instead.\n");
				// set default offset
				mOffset = "2_740.004_753.000_728.820_0.000_0.000_90.000_737.249_2253.118_756.878_0.220_-0.700_89.940_3008_1504_1034";
			}
			else
			{
				offset >> mOffset;
				printf("Read offset: %s\n", mOffset.c_str());
			}
		}
	}
}


HRESULT CGrabBitmap::GetPin( IBaseFilter * pFilter, PIN_DIRECTION dirrequired, int iNum, IPin **ppPin)
{
    CComPtr< IEnumPins > pEnum;
    *ppPin = NULL;

    HRESULT hr = pFilter->EnumPins(&pEnum);
    if(FAILED(hr)) 
        return hr;

    ULONG ulFound;
    IPin *pPin;
    hr = E_FAIL;

    while(S_OK == pEnum->Next(1, &pPin, &ulFound))
    {
        PIN_DIRECTION pindir = (PIN_DIRECTION)3;

        pPin->QueryDirection(&pindir);
        if(pindir == dirrequired)
        {
            if(iNum == 0)
            {
                *ppPin = pPin;  // Return the pin's interface
                hr = S_OK;      // Found requested pin, so clear error
                break;
            }
            iNum--;
        } 

        pPin->Release();
    } 

    return hr;
}


IPin * CGrabBitmap::GetInPin( IBaseFilter * pFilter, int nPin )
{
    CComPtr<IPin> pComPin=0;
    GetPin(pFilter, PINDIR_INPUT, nPin, &pComPin);
    return pComPin;
}


IPin * CGrabBitmap::GetOutPin( IBaseFilter * pFilter, int nPin )
{
    CComPtr<IPin> pComPin=0;
    GetPin(pFilter, PINDIR_OUTPUT, nPin, &pComPin);
    return pComPin;
}

void CGrabBitmap::GetDefaultCapDevice(IBaseFilter **ppCap)
{
	HRESULT hr;

	ASSERT(ppCap);
	if (!ppCap)
		return;
	*ppCap = NULL;
	
	// Create an enumerator
	CComPtr<ICreateDevEnum> pCreateDevEnum;
	pCreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);
	
	ASSERT(pCreateDevEnum);
	if(!pCreateDevEnum)
		return;

	// Enumerate video capture devices
	CComPtr<IEnumMoniker> pEm;
	pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEm, 0);

	ASSERT(pEm);
	if(!pEm)
		return;
	pEm->Reset();

	// Go through and find first capture device
	//
	while (true)
	{
		ULONG ulFetched = 0;
		CComPtr<IMoniker> pM;
		hr = pEm->Next(1, &pM, &ulFetched);
		if(hr != S_OK)
			break;

		// Ask for the actual filter
		hr = pM->BindToObject(0,0,IID_IBaseFilter, (void **)ppCap);
		if(*ppCap)
			break;
	}
	return;
}  
