// Grab.cpp
/*
    Note: Before getting started, Basler recommends reading the Programmer's Guide topic
    in the pylon C++ API documentation that gets installed with pylon.
    If you are upgrading to a higher major version of pylon, Basler also
    strongly recommends reading the Migration topic in the pylon C++ API documentation.

    This sample illustrates how to grab and process images using the CInstantCamera class.
    The images are grabbed and processed asynchronously, i.e.,
    while the application is processing a buffer, the acquisition of the next buffer is done
    in parallel.

    The CInstantCamera class uses a pool of buffers to retrieve image data
    from the camera device. Once a buffer is filled and ready,
    the buffer can be retrieved from the camera object for processing. The buffer
    and additional image data are collected in a grab result. The grab result is
    held by a smart pointer after retrieval. The buffer is automatically reused
    when explicitly released or when the smart pointer object is destroyed.
*/

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#ifdef PYLON_WIN_BUILD
#    include <pylon/PylonGUI.h>
#endif

#define USE_USB
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>
#include <pylon/usb/BaslerUsbDeviceInfo.h>


//#include <pylon/usb/CUsbCameraParams_Params.h>

// Namespace for using pylon objects.
typedef Pylon::CBaslerUsbInstantCamera Camera_t;
typedef Pylon::CBaslerUsbImageEventHandler ImageEventHandler_t; // Or use Camera_t::ImageEventHandler_t
typedef Pylon::CBaslerUsbGrabResultPtr GrabResultPtr_t; // Or use Camera_t::GrabResultPtr_t

#include "../include/ConfigurationEventPrinter.h"
#include "../include/ImageEventPrinter.h"
#include <time.h>

using namespace Pylon;
using namespace std;
using namespace Basler_UsbCameraParams;



enum GrabState { Start, Preview, Burst, Teardown };
enum KeyAction { NoAction, GainIncrease, GainDecrease, ExposureIncrease, ExposureDecrease, BurstGrab, Quit};

// Number of images to be grabbed.
static const uint32_t c_countOfImagesToGrab = 100;


//static CBaslerUsbInstantCamera* camera = 0;
static CGrabResultPtr ptrGrabResult;

static GrabState State = Start;
static KeyAction Action = NoAction;
static double Exposure = 10000.0;
static double ExposureStep = 1.18920711; //2**0.25
static double ColorExposureMultiplier = 2.5;

static double Gain = 0.0;
static double GainStep = 3.0;

// Number of images to be grabbed.
//static const uint32_t c_countOfImagesToGrab = 30;
static const size_t c_maxCamerasToUse = 2;
CBaslerUsbInstantCameraArray* cameras;

static vector<bool> _IsCameraBW(c_maxCamerasToUse, 0);

//Example of an image event handler.
class CSampleImageEventHandler : public CImageEventHandler
{
public:
	virtual void OnImageGrabbed(CInstantCamera& camera, const CGrabResultPtr& ptrGrabResult)
	{
		intptr_t cameraContextValue = ptrGrabResult->GetCameraContext();

		#ifdef PYLON_WIN_BUILD
				// Display the image
			Pylon::DisplayImage(cameraContextValue, ptrGrabResult);
		#endif

		/*cout << "CSampleImageEventHandler::OnImageGrabbed called." << std::endl;
		cout << std::endl;
		cout << std::endl;*/
	}
};

KeyAction ParseKey(char key)
{
	if ((key == 'q' || key == 'Q'))
		return Quit;
	else if ((key == 'b' || key == 'B'))
		return BurstGrab;
	else if (key == 'g')
		return GainDecrease;
	else if (key == 'G')
		return GainIncrease;
	else if (key == 'e')
		return ExposureDecrease;
	else if (key == 'E')
		return ExposureIncrease;
	else return NoAction;
};

void PrintState(char* Head, GrabState State, KeyAction Action)
{
	string StateStr;
	string ActionStr;

	switch (Action)
	{
		case NoAction: ActionStr = "No Action"; break;
		case GainIncrease: ActionStr = "Gain Increase";  break;
		case GainDecrease: ActionStr = "Gain Decrease"; break;
		case ExposureIncrease: ActionStr = "Exposure Increase"; break;
		case ExposureDecrease:  ActionStr = "Exposure Decrease"; break;
		case BurstGrab: ActionStr = "Burst Grab";  break;
		case Quit: ActionStr = "Quit";   break;
		default: break;
	}

	switch (State)
	{
		case Start: StateStr = " Start"; break;
		case Preview: StateStr = " Preview";  break;
		case Burst: StateStr = " Burst"; break;
		case Teardown: StateStr = " Teardown"; break;
		default: break;
	}
	cout << Head << "State:  " << StateStr << "    Action: " << ActionStr << endl;
}

void _Quit(){};

void _StartPreview()
{ 
	// Create an instant camera object with the camera device found first.
	if (State == NoAction)
	{
		//camera = new CBaslerUsbInstantCamera(CTlFactory::GetInstance().CreateFirstDevice());

		//cout << "Using device " << camera->GetDeviceInfo().GetModelName() << endl;
		//camera->MaxNumBuffer = 5;

		//camera.RegisterConfiguration(new CSoftwareTriggerConfiguration, RegistrationMode_ReplaceAll, Cleanup_Delete);
		//camera.RegisterConfiguration(new CConfigurationEventPrinter, RegistrationMode_Append, Cleanup_Delete);
		//camera.RegisterImageEventHandler(new CImageEventPrinter, RegistrationMode_Append, Cleanup_Delete);
		for (size_t i = 0; i < cameras->GetSize(); ++i)
		{
			cameras->operator[](i).Open();
			cameras->operator[](i).GainAuto.SetValue(GainAuto_Off);
			cameras->operator[](i).Gain.SetValue(int(Gain));
			cameras->operator[](i).ExposureTime.SetValue(int(Exposure));

			cameras->operator[](i).RegisterImageEventHandler(new CSampleImageEventHandler, RegistrationMode_Append, Cleanup_Delete);
			cameras->operator[](i).StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);

			//cameras[i].StartGrabbing(c_countOfImagesToGrab, GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
		}
		
		/*
		camera->Open();
		camera->GainAuto.SetValue(GainAuto_Off);
		camera->Gain.SetValue(int(Gain));
		camera->ExposureTime.SetValue(int(Exposure));

		camera->RegisterImageEventHandler(new CSampleImageEventHandler, RegistrationMode_Append, Cleanup_Delete);
		camera->StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);*/
	}


//	if (!(camera->IsGrabbing())) exit(0);
};

void _GainIncrease()
{ 
	Gain = Gain + GainStep; 
	//camera->Gain.SetValue(int(Gain));
	cout << "Camera Gain: " << Gain <<" dB"<< endl;
};

void _GainDecrease()
{
	Gain = Gain - GainStep;
	if (Gain < 0.0) Gain = 0.0;
	//camera->Gain.SetValue(int(Gain)); 
	cout << "Camera Gain: " << Gain << " dB" << endl;
};

void _ExposureIncrease()
{
	Exposure = Exposure * ExposureStep;
	//camera->ExposureTime.SetValue(int(Exposure));
	for (size_t i = 0; i < cameras->GetSize(); ++i)
	{
		if (_IsCameraBW[i])
		{
			cameras->operator[](i).ExposureTime.SetValue(Exposure);
			cout << "Camera BW Exposure: " << Exposure << " mks" << endl;
		}
		else
		{
			cameras->operator[](i).ExposureTime.SetValue(Exposure*ColorExposureMultiplier);
			cout << "Camera Color Exposure: " << Exposure*ColorExposureMultiplier << " mks" << endl;
		}
	}

};

void _ExposureDecrease()
{
	Exposure = Exposure / ExposureStep;
	//camera->ExposureTime.SetValue(int(Exposure));
	//cout << "Camera Exposure: " << Exposure << " mks" << endl;

	for (size_t i = 0; i < cameras->GetSize(); ++i)
	{
		if (_IsCameraBW[i])
		{
			cameras->operator[](i).ExposureTime.SetValue(Exposure);
			cout << "Camera BW Exposure: " << Exposure << " mks" << endl;
		}
		else
		{
			cameras->operator[](i).ExposureTime.SetValue(Exposure*ColorExposureMultiplier);
			cout << "Camera Color Exposure: " << Exposure*ColorExposureMultiplier << " mks" << endl;
		}
	}

};

void _BurstGrab(){};
void _NoAction(){};

GrabState ProcessMessage(GrabState State, KeyAction Action)
{
	PrintState("Was: ", State, Action);

	switch (State)
	{
		case Start:
			switch (Action)
			{
				case NoAction: _StartPreview(); State = Preview; break;
				case GainIncrease: State = Preview; _StartPreview();  break;
				case GainDecrease: State = Preview; _StartPreview(); break;
				case ExposureIncrease: State = Preview; _StartPreview(); break;
				case ExposureDecrease: State = Preview; _StartPreview(); break;
				case BurstGrab: State = Preview; _StartPreview(); break;
				case Quit: _Quit(); break;
				default: break;
			}
			break;
		case Preview:
			switch (Action)
			{
				case NoAction: _NoAction(); break;
				case GainIncrease: _GainIncrease();  break;
				case GainDecrease: _GainDecrease(); break;
				case ExposureIncrease: _ExposureIncrease(); break;
				case ExposureDecrease: _ExposureDecrease(); break;
				case BurstGrab: State = Burst; _BurstGrab(); break;
				case Quit: State = Teardown; _Quit(); break;
				default: break;
			}
			break;
		case Burst:
			switch (Action)
			{
				case NoAction: State = Preview; _StartPreview(); break;
				case GainIncrease: _NoAction(); break;
				case GainDecrease: _NoAction(); break;
				case ExposureIncrease:  _NoAction(); break;
				case ExposureDecrease:  _NoAction(); break;
				case BurstGrab:  _NoAction(); break;
				case Quit: State = Teardown; _Quit(); break;
				default: break;
			}
			break;
		case Teardown:
			switch (Action)
			{
				case NoAction: break;
				case GainIncrease: break;
				case GainDecrease: break;
				case ExposureIncrease: break;
				case ExposureDecrease: break;
				case BurstGrab: break;
				case Quit: break;
				default: break;
			}
			break;
		default: break;
	}

	PrintState("Is now: ", State, Action);

	return State;
}

int main(int argc, char* argv[])
{
    // The exit code of the sample application.
    int exitCode = 0;

    // Automagically call PylonInitialize and PylonTerminate to ensure the pylon runtime system
    // is initialized during the lifetime of this object.
    Pylon::PylonAutoInitTerm autoInitTerm;
	


	CDeviceInfo info;
	info.SetDeviceClass(Camera_t::DeviceClass());
	CTlFactory& tlFactory = CTlFactory::GetInstance();
	DeviceInfoList_t devices;
	
	if (tlFactory.EnumerateDevices(devices) == 0)
	{
		throw RUNTIME_EXCEPTION("No camera present.");
	}

	int cam_num = devices.size();

	cameras = new CBaslerUsbInstantCameraArray(min(devices.size(), c_maxCamerasToUse));
	// Create and attach all Pylon Devices.
	for (size_t i = 0; i < cameras->GetSize(); ++i)
	{
		cameras->operator[](i).Attach(tlFactory.CreateDevice(devices[i]));

		//cameras->operator[](i).RegisterConfiguration(new CSoftwareTriggerConfiguration, RegistrationMode_ReplaceAll, Cleanup_Delete);
		//cameras->operator[](i).RegisterConfiguration(new CConfigurationEventPrinter, RegistrationMode_Append, Cleanup_Delete);

		//cameras->operator[](i).RegisterImageEventHandler(new CImageEventPrinter, RegistrationMode_Append, Cleanup_Delete);
		cameras->operator[](i).RegisterImageEventHandler(new CSampleImageEventHandler, RegistrationMode_Append, Cleanup_Delete);

		cameras->operator[](i).Open();


		CDeviceInfo & diRef = devices[i];

		if (diRef.GetModelName().find(GenICam::gcstring("acA2500-14uc")) != GenICam::gcstring::_npos())
		{
			_IsCameraBW[i] = false;
		}
		else if (diRef.GetModelName().find(GenICam::gcstring("acA2500-14um")) != GenICam::gcstring::_npos())
		{
			_IsCameraBW[i] = true;
		}

		//cameras->operator[](i).PixelFormat.SetValue(PixelFormat_Mono12);

		cameras->operator[](i).Gain.SetValue(0);
	
		if (_IsCameraBW[i])
			cameras->operator[](i).ExposureTime.SetValue(Exposure);
		else 
			cameras->operator[](i).ExposureTime.SetValue(Exposure*ColorExposureMultiplier);

		// Print the model name of the camera.
		cout << "Using device " << cameras->operator[](i).GetDeviceInfo().GetModelName() << endl;



		if (GenApi::IsWritable(cameras->operator[](i).ChunkModeActive))
		{
			cameras->operator[](i).ChunkModeActive.SetValue(true);
		}
		else
		{
			throw RUNTIME_EXCEPTION("The camera doesn't support chunk features");
		}

		// Enable time stamp chunks.
		cameras->operator[](i).ChunkSelector.SetValue(ChunkSelector_Timestamp);
		cameras->operator[](i).ChunkEnable.SetValue(true);

	}


    try
    {
     
		char key;
	
		State = ProcessMessage(State, Action);

		do
		{
			cin.get(key);
			Action = ParseKey(key);
			State = ProcessMessage(State, Action);

		} while (Action != Quit);

    }
    catch (GenICam::GenericException &e)
    {
        // Error handling.
        cerr << "An exception occurred." << endl
        << e.GetDescription() << endl;
        exitCode = 1;
    }

    // Comment the following two lines to disable waiting on exit.
    cerr << endl << "Press Enter to exit." << endl;
    while( cin.get() != '\n');

    return exitCode;
}
