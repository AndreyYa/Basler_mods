
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
static const uint32_t c_countOfImagesToGrab = 15;
//static CGrabResultPtr ptrGrabResult;

static GrabState G_State = Start;
static KeyAction Action = NoAction;
static double Exposure = 10000.0;
static double ExposureStep = 1.18920711; //2**0.25
static double ColorExposureMultiplier = 2.5;
static int BurstCounter = 0;
static char filename[] = "Burst";

static double Gain = 0.0;
static double GainStep = 3.0;

CBaslerUsbInstantCameraArray* cameras;

static vector<vector<double>> _PC_frame_time_table(c_maxCamerasToUse * 2, vector<double>(c_countOfImagesToGrab, 0.0));
static vector<bool> _IsCameraBW(c_maxCamerasToUse, 0);
static vector<int> _PC_triggered_frame_count(c_maxCamerasToUse, 0);
static vector<int> _PC_captured_frame_count(c_maxCamerasToUse, 0);

class MyBufferFactory;
static vector<MyBufferFactory *> _ImageBuffers(c_maxCamerasToUse);


static vector<vector<CBaslerUsbGrabResultPtr>> _Grab_results(c_maxCamerasToUse, vector<CBaslerUsbGrabResultPtr>(c_countOfImagesToGrab));


void ProcessMessage(KeyAction Action);
void PrintTimeTable();
void _StoreFrames(int, char*);

class MyBufferFactory : public IBufferFactory
{
public:
	MyBufferFactory()
		: m_lastBufferContext(1000)
	{
	}

	virtual ~MyBufferFactory()
	{
	}

	virtual void AllocateBuffer(size_t bufferSize, void** pCreatedBuffer, intptr_t& bufferContext)
	{
		try
		{
			*pCreatedBuffer = new uint8_t[bufferSize];
			bufferContext = ++m_lastBufferContext;

			cout << "Created buffer " << bufferContext << ", " << *pCreatedBuffer << endl;
		}
		catch (const std::exception&)
		{
			if (*pCreatedBuffer != NULL)
			{
				delete[] * pCreatedBuffer;
				*pCreatedBuffer = NULL;
			}

			throw;
		}
	}

	virtual void FreeBuffer(void* pCreatedBuffer, intptr_t bufferContext)
	{
		uint8_t* p = reinterpret_cast<uint8_t*>(pCreatedBuffer);
		delete[] p;
		cout << "Freed buffer " << bufferContext << ", " << pCreatedBuffer << endl;
	}

	virtual void DestroyBufferFactory()
	{
		delete this;
	}

protected:
	unsigned long m_lastBufferContext;
};



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
			_frame_index = _PC_captured_frame_count[cameraContextValue];
			
			cout << "Frame Grabbed      #: " << _frame_index << endl;

			if (IsReadable(ptrGrabResultUsb->ChunkTimestamp))
			{
				_PC_frame_time_table[2 * cameraContextValue + 1][_frame_index] = 0.000000001*(double)ptrGrabResultUsb->ChunkTimestamp.GetValue();
			}
			
			
			_Grab_results[cameraContextValue][_frame_index] = ptrGrabResultUsb;

			

			_PC_captured_frame_count[cameraContextValue] += 1;

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
		if (_IsCameraBW[i])
			cameras->operator[](i).ExposureTime.SetValue(Exposure);
		else
			cameras->operator[](i).ExposureTime.SetValue(Exposure*ColorExposureMultiplier);
		cameras->operator[](i).MaxNumBuffer = c_countOfImagesToGrab;

		_PC_triggered_frame_count[i] = 0;
		_PC_captured_frame_count[i] = 0;

		CSoftwareTriggerConfiguration().OnOpened(cameras->operator[](i));
		cameras->operator[](i).StartGrabbing(c_countOfImagesToGrab, GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
	}

	for (size_t j = 0; j < c_countOfImagesToGrab; ++j)
	{
		for (size_t i = 0; i < cameras->GetSize(); ++i)
		{
			if (cameras->operator[](i).WaitForFrameTriggerReady(1000, TimeoutHandling_ThrowException))
			{
				cameras->operator[](i).ExecuteSoftwareTrigger();


				_PC_triggered_frame_count[i] = j;
				cout << "Frame Triggered    #: " << _PC_triggered_frame_count[i] << endl;

				WaitObject::Sleep(10);
				_PC_frame_time_table[2 * i][j] = (double)clock() / CLOCKS_PER_SEC;
			}
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

	BurstCounter++;
	//_StoreFrames(BurstCounter, filename);

	for (size_t j = 0; j < c_countOfImagesToGrab; ++j)
	{
		for (size_t i = 0; i < cameras->GetSize(); ++i)
		{
			_Grab_results[i][j].Release();
		}
	}

	ProcessMessage(NoAction);
};

void _StoreFrames(int Label, char *filename)
{

	for (size_t i = 0; i < cameras->GetSize(); ++i)
	{
		char camSerialNumber[100];
		char camColor[50];
		
		if (_IsCameraBW[i] == true)
		{
			sprintf(camColor, "BW");
		}
		else
		{
			sprintf(camColor, "Color");
		}

		for (size_t j = 0; j < c_countOfImagesToGrab; ++j)
		{	
			if (_Grab_results[i][j]->GrabSucceeded())
			{
				const uint16_t *pImageBuffer = (uint16_t *)(_Grab_results[i][j]->GetBuffer());
				size_t VbufferSize = _Grab_results[i][j]->GetImageSize();
				void* Vbuffer = _Grab_results[i][j]->GetBuffer();
				uint32_t Vwidth = _Grab_results[i][j]->GetWidth();
				uint32_t Vheight = _Grab_results[i][j]->GetHeight();
				FILE* pFile;
				char raw_filename[512];
				sprintf(raw_filename, "%s-%s-%iX%i-%u-%d-%d.raw", filename, camColor, Vwidth, Vheight, camSerialNumber, Label, j);
				pFile = fopen(raw_filename, "wb");
				if (pFile) fwrite(Vbuffer, 1, VbufferSize, pFile);
				else printf("Can't open file");
				fclose(pFile);

				CPylonImage imageToSave;
				imageToSave.AttachGrabResultBuffer(_Grab_results[i][j]);

				char bmp_filename[512];
				sprintf(bmp_filename, "%s-%iX%i-%u-%d-%d.png", filename, Vwidth, Vheight, camSerialNumber, Label, j);

				imageToSave.Save(ImageFileFormat_Png, bmp_filename);

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

void _StopPreview()
{
	for (size_t i = 0; i < cameras->GetSize(); ++i)
	{
		if (cameras->operator[](i).IsGrabbing())
			cameras->operator[](i).StopGrabbing();
	}
}

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

			cameras->operator[](i).MaxNumBuffer = 2;

			CAcquireContinuousConfiguration().OnOpened(cameras->operator[](i));
			cameras->operator[](i).StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
		}
	}
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
				case BurstGrab: _StopPreview(); G_State = Burst; _BurstGrab(); break;
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


		

		_ImageBuffers[i] = new MyBufferFactory();
		cameras->operator[](i).SetBufferFactory(_ImageBuffers[i], Cleanup_None);

		CDeviceInfo & diRef = devices[i];

		if (diRef.GetModelName().find(GenICam::gcstring("acA2500-14uc")) != GenICam::gcstring::_npos())
		{
			_IsCameraBW[i] = false;
			cameras->operator[](i).PixelFormat.SetValue(PixelFormat_BayerGB12);
		}
		else if (diRef.GetModelName().find(GenICam::gcstring("acA2500-14um")) != GenICam::gcstring::_npos())
		{
			_IsCameraBW[i] = true;
			cameras->operator[](i).PixelFormat.SetValue(PixelFormat_Mono12);
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
