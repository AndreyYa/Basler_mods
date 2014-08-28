// Grab.cpp


// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#ifdef PYLON_WIN_BUILD
#    include <pylon/PylonGUI.h>
#endif

// Namespace for using pylon objects.
using namespace Pylon;

// Namespace for using cout.
using namespace std;

// Settings to use  Basler USB cameras.
#include <pylon/usb/BaslerUsbInstantCamera.h>
//typedef Pylon::CBaslerUsbInstantCamera Camera_t;
using namespace Basler_UsbCameraParams;

//bool ConfigureCamera()

// Number of images to be grabbed.
static const uint32_t c_countOfImagesToGrab = 2;
int CaptureImages(CDeviceInfo CameraID, PixelFormatEnums PixFormat,int Gain, int ShutterMks, int CapturesNmb,char *filename,  int Counter )
	{
		char camSerialNumber[100];
		CBaslerUsbInstantCamera _Camera(CTlFactory::GetInstance().CreateFirstDevice(CameraID));
		_Camera.Open();
				
		sprintf(camSerialNumber,"%s", _Camera.DeviceUserID.GetValue().c_str() );

		if ( GenApi::IsAvailable( _Camera.PixelFormat.GetEntry(PixFormat)))
			_Camera.PixelFormat.SetValue(PixFormat);

		_Camera.Gain.SetValue(Gain);
		_Camera.ExposureTime.SetValue(ShutterMks);

		_Camera.MaxNumBuffer = 10;
		_Camera.StartGrabbing(CapturesNmb);

		CGrabResultPtr ptrGrabResult;
		int imageCounter = 0;

		while ( _Camera.IsGrabbing())
		{
			_Camera.RetrieveResult( 10000, ptrGrabResult, TimeoutHandling_ThrowException);
			if (ptrGrabResult->GrabSucceeded())
			{
				const uint16_t *pImageBuffer = (uint16_t *) ptrGrabResult->GetBuffer();
				size_t VbufferSize = ptrGrabResult->GetImageSize();
				void* Vbuffer = ptrGrabResult->GetBuffer();
				uint32_t Vwidth = ptrGrabResult->GetWidth();
				uint32_t Vheight = ptrGrabResult->GetHeight();
				FILE* pFile;
				char raw_filename[512];
				sprintf( raw_filename, "%s-%iX%i-%u-%d-%d.raw",filename,Vwidth,Vheight, camSerialNumber, Counter, imageCounter);
				pFile = fopen(raw_filename,"wb");
				if (pFile ) fwrite(Vbuffer,1,VbufferSize,pFile); 
				else printf("Can't open file");
				fclose(pFile);

				CPylonImage imageToSave;
				imageToSave.AttachGrabResultBuffer( ptrGrabResult);

				char bmp_filename[512];
				sprintf( bmp_filename, "%s-%iX%i-%u-%d-%d.png",filename,Vwidth,Vheight, camSerialNumber, Counter, imageCounter);

				imageToSave.Save( ImageFileFormat_Png, bmp_filename);


				imageCounter++;
			}
			else cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << endl;
		}
		
		return 0;
	}


int main(int argc, char* argv[])
{
    // The exit code of the sample application.
    int exitCode = 0;

	enum ECameraColorType
    {
		CameraColorType_NotDefined,
		CameraColorType_BW,
		CameraColorType_Color
	};

	ECameraColorType _CameraColorTypeRequested = CameraColorType_NotDefined;
	ECameraColorType _CameraColorTypeFound = CameraColorType_NotDefined;

	int _shutter = 0;
	int _gain = -10;
	int _capture_nmb = 0;
	char filename[100];
	char camType[100];
	char camSerialNumber[100];
	int _counter = 0;

	std::string iT("-t");
	std::string iG("-g");
	std::string iN("-n");
	std::string iF("-f");
	std::string iC("-c");
	std::string iB("-b");

	if (argc < 5) 
	{ // Check the value of argc. If not enough parameters have been passed, inform user and exit.
        std::cout << "Usage is -t <shutter_time_mks> -g <gain_db> -n <num_to_capture> -f <filename> -c <counter_number> -b <color/bw>\n"; // Inform the user of how to use the program
        std::cin.get();
        exit(0);
    }
	else 
	{
		std::cout << argv[0];

        for (int i = 1; i < argc; i++)
		{
			std::string iArg = argv[i];

            if (i + 1 != argc)
			{// Check that we haven't finished parsing already
				

				if (/*iArg == iT*/iArg.compare(iT) == 0) 
				{
                    // We know the next argument *should* be the filename:
                    _shutter = atoi(argv[i + 1]);
                } 
				else if (iArg.compare(iG) == 0) 
				{
                    _gain = atoi(argv[i + 1]);
                } 
				else if (iArg.compare(iN) == 0) 
				{
                    _capture_nmb = atoi(argv[i + 1]);
                }
				else if (iArg.compare(iF) == 0) 
				{
					strcpy(filename,argv[i + 1]);
		        }
				else if (iArg.compare(iC) == 0) 
				{
                    _counter = atoi(argv[i + 1]);
                }
				else if (iArg.compare(iB) == 0) 
				{
                    strcpy(camType,argv[i + 1]);
					std::string s_BW("bw");
					std::string s_Color("color");
					if(s_BW.compare(camType) == 0)
						_CameraColorTypeRequested = CameraColorType_BW;
					else if (s_Color.compare(camType) == 0)
						_CameraColorTypeRequested = CameraColorType_Color;
                }
				else 
				{

                    //std::cout << "Not enough or invalid arguments, please try again.\n";
                    
                    //exit(1);
				}
				std::cout << argv[i] << " " ;
			}
        }
		cout << endl;
	};

	

    // Automagically call PylonInitialize and PylonTerminate to ensure the pylon runtime system
    // is initialized during the lifetime of this object.
    Pylon::PylonAutoInitTerm autoInitTerm;

    try
    {
		int c_maxCamerasToUse = 5;

		CTlFactory& tlFactory = CTlFactory::GetInstance();

		DeviceInfoList_t devices;
        if ( tlFactory.EnumerateDevices(devices) == 0 ) throw RUNTIME_EXCEPTION( "No camera present.");
     
		CBaslerUsbInstantCamera  *_pCamera = NULL;
		
		for (DeviceInfoList::iterator it = devices.begin(); it != devices.end();  ++it)
        {
            CDeviceInfo di = *it;
			CDeviceInfo & diRef = di;
		
			if (diRef.GetModelName().find(GenICam::gcstring("acA2500-14uc")) != GenICam::gcstring::_npos())
            {
				_CameraColorTypeFound = CameraColorType_Color;
            }
			else if (diRef.GetModelName().find(GenICam::gcstring("acA2500-14um")) != GenICam::gcstring::_npos())
			{
				_CameraColorTypeFound = CameraColorType_BW;
            }
		
			if(_CameraColorTypeFound == CameraColorType_Color) 
			{
				if(_CameraColorTypeRequested == CameraColorType_Color)//configure camera color
				{
					 CaptureImages(di,PixelFormat_BayerGB12,_gain,_shutter,_capture_nmb,filename,_counter);
				}
				else if(_CameraColorTypeRequested ==CameraColorType_BW)
				{
					//skip camera configuration, look for another camera
				}
				else if(_CameraColorTypeRequested == CameraColorType_NotDefined)
				{
					//skip camera configuration, look for another camera
				}
			}
			else if(_CameraColorTypeFound == CameraColorType_BW)
			{
				if(_CameraColorTypeRequested == CameraColorType_BW)
				{
					 CaptureImages(di,PixelFormat_Mono12,_gain,_shutter,_capture_nmb,filename,_counter);
					//configure camera BW
					/*_pCamera = new CBaslerUsbInstantCamera( CTlFactory::GetInstance().CreateFirstDevice(di));
					_pCamera->Open();
					_pCamera->PixelFormat.SetValue(PixelFormat_Mono12);
					_pCamera->Gain.SetValue(_gain);
					_pCamera->ExposureTime.SetValue(_shutter*1000.0);
					*/
				}
				else if(_CameraColorTypeRequested == CameraColorType_Color)
				{
					//skip camera configuration, look for another camera
				}
				else if(_CameraColorTypeRequested == CameraColorType_NotDefined)
				{
					//skip camera configuration, look for another camera
				}
			}
			else if(_CameraColorTypeFound == CameraColorType_BW)
			{
				//skip camera configuration, look for another camera
			}

        }

        
    }
    catch (GenICam::GenericException &e)
    {
        // Error handling.
        cerr << "An exception occurred." << endl
        << e.GetDescription() << endl;
	        exitCode = 1;
    }

    // Comment the following two lines to disable waiting on exit.
    //cerr << endl << "Press Enter to exit." << endl;
    //while( cin.get() != '\n');

    return exitCode;
}
