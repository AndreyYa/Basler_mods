
// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/PylonGUI.h>


#define USE_USB
using namespace Pylon;

#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>
//#include <pylon/usb/BaslerUsbDeviceInfo.h>
typedef Pylon::CBaslerUsbInstantCamera Camera_t;
typedef Pylon::CBaslerUsbImageEventHandler ImageEventHandler_t; // Or use Camera_t::ImageEventHandler_t
typedef Pylon::CBaslerUsbGrabResultPtr GrabResultPtr_t; // Or use Camera_t::GrabResultPtr_t
using namespace Basler_UsbCameraParams;
#include "../include/ConfigurationEventPrinter.h"
#include "../include/ImageEventPrinter.h"


using namespace std;


#include <time.h>

enum GrabState { Start, Preview, Burst, Teardown };
enum KeyAction { NoAction, GainIncrease, GainDecrease, ExposureIncrease, ExposureDecrease, BurstGrab, Quit};

static const size_t c_maxCamerasToUse = 1;
static const uint32_t c_countOfImagesToGrab = 25;
static CGrabResultPtr ptrGrabResult;

static GrabState G_State = Start;
static KeyAction Action = NoAction;
static double Exposure = 10000.0;
static double ExposureStep = 1.18920711; //2**0.25
static double ColorExposureMultiplier = 2.5;

static double Gain = 0.0;
static double GainStep = 3.0;

CBaslerUsbInstantCameraArray* cameras;

static vector<vector<double>> _PC_frame_time_table(c_maxCamerasToUse * 2, vector<double>(c_countOfImagesToGrab, 0.0));
static vector<bool> _IsCameraBW(c_maxCamerasToUse, 0);
static vector<int> _PC_frame_count(c_maxCamerasToUse, 0);

void ProcessMessage(KeyAction Action);

class CSampleImageEventHandler : public CImageEventHandler
{
public:
	virtual void OnImageGrabbed(CInstantCamera& camera, const CGrabResultPtr& ptrGrabResult)
	{
		intptr_t cameraContextValue = ptrGrabResult->GetCameraContext();

		CBaslerUsbGrabResultPtr ptrGrabResultUsb = ptrGrabResult;

		#ifdef PYLON_WIN_BUILD
		Pylon::DisplayImage(cameraContextValue, ptrGrabResultUsb);
		#endif

			if (G_State == Burst)
			{
				
				int _frame_index = 0;
				_frame_index = _PC_frame_count[cameraContextValue];

				if (IsReadable(ptrGrabResultUsb->ChunkTimestamp))
				{
					_PC_frame_time_table[2 * cameraContextValue + 1][_frame_index] = 0.000000001*(double)ptrGrabResultUsb->ChunkTimestamp.GetValue();
				}
				//cout << "++++++  Burst: OnImageGrabbed Frame#: " << _frame_index  << endl;

				//_PC_frame_count[cameraContextValue] += 1;
				if (camera.WaitForFrameTriggerReady(300, TimeoutHandling_ThrowException))
				{
					camera.ExecuteSoftwareTrigger();
					
					if (_frame_index < c_countOfImagesToGrab - 1)
					{
						_PC_frame_count[cameraContextValue] += 1;

						_frame_index = _PC_frame_count[cameraContextValue];

						_PC_frame_time_table[2 * cameraContextValue][_frame_index] = (double)clock() / CLOCKS_PER_SEC;
					}
				}
			}
	}
};


void PrintTimeTable()
{
	for (size_t i = 0; i < c_countOfImagesToGrab - 1; ++i)
	{
		cout << "Camera";
		//for (size_t j = 0; j < c_maxCamerasToUse; ++j)
		for (size_t j = 0; j < c_maxCamerasToUse; ++j)
		{
			cout << " #" << j << ": " << _PC_frame_time_table[2 * j][i + 1] - _PC_frame_time_table[2 * j][i] << " Cam Time:" << _PC_frame_time_table[2 * j + 1][i + 1] - _PC_frame_time_table[2 * j + 1][i];
		}
		cout << endl;
	}
}


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
	if (G_State == NoAction || G_State == Preview)
	{
		cout << "++++++  Preview: Start" << endl;

		for (size_t i = 0; i < cameras->GetSize(); ++i)
		{
			if (cameras->operator[](i).IsGrabbing())
				cameras->operator[](i).StopGrabbing();

			cameras->operator[](i).Open();
			cameras->operator[](i).GainAuto.SetValue(GainAuto_Off);
			cameras->operator[](i).Gain.SetValue(int(Gain));
		
			if (_IsCameraBW[i])
				cameras->operator[](i).ExposureTime.SetValue(Exposure);
			else
				cameras->operator[](i).ExposureTime.SetValue(Exposure*ColorExposureMultiplier);

			cameras->operator[](i).MaxNumBuffer = 5;

			CAcquireContinuousConfiguration().OnOpened(cameras->operator[](i));
			cameras->operator[](i).StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
		}
	}
};


void _BurstGrab()
{
	for (size_t i = 0; i < cameras->GetSize(); ++i)
	{
		if (cameras->operator[](i).IsGrabbing())
			cameras->operator[](i).StopGrabbing();

		cameras->operator[](i).Open();
		cameras->operator[](i).GainAuto.SetValue(GainAuto_Off);
		cameras->operator[](i).Gain.SetValue(int(Gain));
		cameras->operator[](i).ExposureTime.SetValue(int(Exposure));
		cameras->operator[](i).MaxNumBuffer = 5;

		CSoftwareTriggerConfiguration().OnOpened(cameras->operator[](i));
		cameras->operator[](i).StartGrabbing(c_countOfImagesToGrab, GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
		
	}

	for (size_t i = 0; i < cameras->GetSize(); ++i)
	{
		if (cameras->operator[](i).WaitForFrameTriggerReady(1000, TimeoutHandling_ThrowException))
		{
			cameras->operator[](i).ExecuteSoftwareTrigger();

			_PC_frame_count[i] = 0;
			int _frame_index = _PC_frame_count[i];
			
			_PC_frame_time_table[2 * i][0] = (double)clock() / CLOCKS_PER_SEC;
		}
	}

	unsigned char IsBurst = 1;

	while (IsBurst > 0)
	{
		for (size_t i = 0; i < cameras->GetSize(); ++i)
		{
			IsBurst = 0;
			WaitObject::Sleep(50);
			if (cameras->operator[](i).IsGrabbing()) IsBurst++;
		}
	}
	
	PrintTimeTable();

	ProcessMessage(NoAction);
};

void _GainIncrease()
{ 
	Gain = Gain + GainStep; 
	if (Gain > 23.79 ) Gain = 21.0;
	for (size_t i = 0; i < cameras->GetSize(); ++i)
	{
		cameras->operator[](i).Gain.SetValue(Gain);
	}
	cout << "Camera Gain: " << Gain << " dB" << endl;
};

void _GainDecrease()
{
	Gain = Gain - GainStep;
	if (Gain < 0.0) Gain = 0.0;
	for (size_t i = 0; i < cameras->GetSize(); ++i)
	{
		cameras->operator[](i).Gain.SetValue(Gain);
	}
	cout << "Camera Gain: " << Gain << " dB" << endl;
};

void _ExposureIncrease()
{
	Exposure = Exposure * ExposureStep;
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

void _NoAction(){};

void ProcessMessage(KeyAction Action)
{
	PrintState("Was: ", G_State, Action);

	switch (G_State)
	{
		case Start:
			switch (Action)
			{
				case NoAction: _StartPreview(); G_State = Preview; break;
				case GainIncrease: G_State = Preview; _StartPreview();  break;
				case GainDecrease: G_State = Preview; _StartPreview(); break;
				case ExposureIncrease: G_State = Preview; _StartPreview(); break;
				case ExposureDecrease: G_State = Preview; _StartPreview(); break;
				case BurstGrab: G_State = Burst; _BurstGrab(); break;
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
				case BurstGrab: G_State = Burst; _BurstGrab(); break;
				case Quit: G_State = Teardown; _Quit(); break;
				default: break;
			}
			break;
		case Burst:
			switch (Action)
			{
				case NoAction: G_State = Preview; _StartPreview(); break;
				case GainIncrease: _NoAction(); break;
				case GainDecrease: _NoAction(); break;
				case ExposureIncrease:  _NoAction(); break;
				case ExposureDecrease:  _NoAction(); break;
				case BurstGrab:  _NoAction(); break;
				case Quit: G_State = Teardown; _Quit(); break;
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

	PrintState("Is now: ", G_State, Action);
	return;
}

int main(int argc, char* argv[])
{

    int exitCode = 0;
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
		cameras->operator[](i).RegisterConfiguration(new CSoftwareTriggerConfiguration, RegistrationMode_Append, Cleanup_Delete);
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

		//cameras->operator[](i).Gain.SetValue(0);
	
		if (_IsCameraBW[i])
			cameras->operator[](i).ExposureTime.SetValue(Exposure);
		else 
			cameras->operator[](i).ExposureTime.SetValue(Exposure*ColorExposureMultiplier);
		cout << "Using device " << cameras->operator[](i).GetDeviceInfo().GetModelName() << endl;

		if (GenApi::IsWritable(cameras->operator[](i).ChunkModeActive))
		{
			cameras->operator[](i).ChunkModeActive.SetValue(true);
		}
		else
		{
			throw RUNTIME_EXCEPTION("The camera doesn't support chunk features");
		}

		cameras->operator[](i).ChunkSelector.SetValue(ChunkSelector_Timestamp);
		cameras->operator[](i).ChunkEnable.SetValue(true);
	}

    try
    {    
		char key;
	
		ProcessMessage(Action);
		do
		{
			cin.get(key);
			Action = ParseKey(key);
			ProcessMessage(Action);

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
