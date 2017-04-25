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

	// This is the implementation function that writes the captured video
	// data onto a bitmap on the user's disk.
	//
    int CopyBitmap(double SampleTime, BYTE * pBuffer, long BufferSize)
    {
        //
        // Convert the buffer into a bitmap
        //
        TCHAR szFilename[MAX_PATH];
        (void)StringCchPrintf(szFilename, NUMELMS(szFilename), TEXT("Bitmap%5.5d.bmp\0"), long( SampleTime * 1000 ) );

        // Create a file to hold the bitmap
        HANDLE hf = CreateFile(szFilename, GENERIC_WRITE, FILE_SHARE_READ, 
                               NULL, CREATE_ALWAYS, NULL, NULL );

        if( hf == INVALID_HANDLE_VALUE )
        {
            return 0;
        }

        // Write out the file header
        //
        BITMAPFILEHEADER bfh;
        memset( &bfh, 0, sizeof( bfh ) );
        bfh.bfType = 'MB';
        bfh.bfSize = sizeof( bfh ) + BufferSize + sizeof( BITMAPINFOHEADER );
        bfh.bfOffBits = sizeof( BITMAPINFOHEADER ) + sizeof( BITMAPFILEHEADER );

        DWORD Written = 0;
        WriteFile( hf, &bfh, sizeof( bfh ), &Written, NULL );
   
        Written = 0;
        WriteFile( hf, &(cbInfo.bih), sizeof( cbInfo.bih ), &Written, NULL );

        // Write the bitmap bits
        //
        Written = 0;
        WriteFile( hf, pBuffer, BufferSize, &Written, NULL );

        CloseHandle( hf );

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

int CGrabBitmap::GrabBitmap(PBITMAPINFO *Bitmap, ULONG *BitmapSize)
{
    USES_CONVERSION;
    CComPtr< ISampleGrabber > pGrabber;
    CComPtr< IBaseFilter >    pSource;
    CComPtr< IGraphBuilder >  pGraph;
    CComPtr< IVideoWindow >   pVideoWindow;
    HRESULT hr;


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
    GrabType.SetSubtype( &MEDIASUBTYPE_RGB24 );
    hr = pGrabber->SetMediaType( &GrabType );

    // Get the output pin and the input pin
    //
    CComPtr< IPin > pSourcePin;
    CComPtr< IPin > pGrabPin;

    pSourcePin = GetOutPin( pSource, 0 );
    pGrabPin   = GetInPin( pGrabberBase, 0 );

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
    cbInfo.bih.biBitCount = 24;

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
        
    // callback got the sample
    CHAR *BitmapData = NULL;
	cbInfo.biSize = CalcBitmapInfoSize(cbInfo.bih);
	ULONG Size = cbInfo.biSize + cbInfo.lBufferSize;
	*BitmapSize = Size;

	if(Bitmap)		// If we have a valid address from caller
	{
		*Bitmap = (BITMAPINFO *) new BYTE[Size];
		if(*Bitmap)
		{
			(**Bitmap).bmiHeader = cbInfo.bih;
			BitmapData = (CHAR *)(*Bitmap) + cbInfo.biSize;
			memcpy(BitmapData, cbInfo.pBuffer, cbInfo.lBufferSize);
		}
	}

    return cbInfo.lBufferSize;
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

void CGrabBitmap::SaveData()
{
	CB.CopyBitmap(cbInfo.dblSampleTime, cbInfo.pBuffer, cbInfo.lBufferSize);
}

ULONG CGrabBitmap::CalcBitmapInfoSize(const BITMAPINFOHEADER &bmiHeader)
{
	UINT bmiSize = (bmiHeader.biSize != 0) ? bmiHeader.biSize : sizeof(BITMAPINFOHEADER);
	return bmiSize + bmiHeader.biClrUsed * sizeof(RGBQUAD);
}
