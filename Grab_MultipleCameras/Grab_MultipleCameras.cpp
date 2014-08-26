// Grab_MultipleCameras.cpp
/*
This sample illustrates how to grab and process images from multiple cameras
using the CInstantCameraArray class. The CInstantCameraArray class represents
an array of instant camera objects. It provides almost the same interface
as the instant camera for grabbing.
The main purpose of the CInstantCameraArray is to simplify waiting for images and
camera events of multiple cameras in one thread. This is done by providing a single
RetrieveResult method for all cameras in the array.
Alternatively, the grabbing can be started using the internal grab loop threads
of all cameras in the CInstantCameraArray. The grabbed images can then be processed by one or more
image event handlers. Please note that this is not shown in this example.
*/

// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#ifdef PYLON_WIN_BUILD
#    include <pylon/PylonGUI.h>
#endif

#define USE_USB

#if defined(USE_USB)
// Settings to use Basler USB cameras.
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>
typedef Pylon::CBaslerUsbInstantCamera Camera_t;
typedef Pylon::CBaslerUsbImageEventHandler ImageEventHandler_t; // Or use Camera_t::ImageEventHandler_t
typedef Pylon::CBaslerUsbGrabResultPtr GrabResultPtr_t; // Or use Camera_t::GrabResultPtr_t
using namespace Basler_UsbCameraParams;
#endif

#include "../include/ConfigurationEventPrinter.h"
#include "../include/ImageEventPrinter.h"

#include <time.h>

// Namespace for using pylon objects.
using namespace Pylon;

// Namespace for using cout.
using namespace std;

// Number of images to be grabbed.
static const uint32_t c_countOfImagesToGrab = 20;
static const size_t c_maxCamerasToUse = 1;

static vector<int64_t> _CurrTimestamp(c_maxCamerasToUse, 0);
static vector<int64_t> _PrevTimestamp(c_maxCamerasToUse, 0);

static vector<double> _PC_frame_start(c_maxCamerasToUse, 0.0);
static vector<double> _PC_frame_stop(c_maxCamerasToUse, 0.0);
static vector<int> _PC_frame_count(c_maxCamerasToUse, 0);

static vector<vector<double>> _PC_frame_time_table(c_maxCamerasToUse*2, vector<double>(c_countOfImagesToGrab, 0.0));

static char IsBurstStarted = 0;
static int c_FrameSetTriggered = -1;

class CSampleImageEventHandler : public CBaslerUsbImageEventHandler //CImageEventHandler //CBaslerUsbImageEventHandler
{
public:
	virtual void OnImageGrabbed(CBaslerUsbInstantCamera& camera, const CBaslerUsbGrabResultPtr& ptrGrabResult)
	{
		intptr_t cameraContextValue = ptrGrabResult->GetCameraContext();
		_PC_frame_stop[cameraContextValue] = (double)clock() / CLOCKS_PER_SEC;
		Pylon::DisplayImage(cameraContextValue, ptrGrabResult);

		if (PayloadType_ChunkData != ptrGrabResult->GetPayloadType()) throw RUNTIME_EXCEPTION("Unexpected payload type received.");


		if (_PC_frame_count[cameraContextValue] < c_countOfImagesToGrab)
		{
			
			int _frame_index = _PC_frame_count[cameraContextValue];
			_PC_frame_time_table[2*cameraContextValue][_frame_index] = _PC_frame_stop[cameraContextValue];// (double)clock() / CLOCKS_PER_SEC;
			
			if (IsReadable(ptrGrabResult->ChunkTimestamp))
			{
				_PC_frame_time_table[2*cameraContextValue+1][_frame_index] = 0.000000001*(double)ptrGrabResult->ChunkTimestamp.GetValue();
			}

			_PC_frame_count[cameraContextValue] += 1;
			_PrevTimestamp[cameraContextValue] = _CurrTimestamp[cameraContextValue];

			//camera.ExecuteSoftwareTrigger();
			_PC_frame_start[cameraContextValue] = (double)clock() / CLOCKS_PER_SEC;

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
			cout << " #" << j << ": " << _PC_frame_time_table[2*j][i + 1] - _PC_frame_time_table[2*j][i] << " Cam Time:" << _PC_frame_time_table[2*j + 1][i + 1] - _PC_frame_time_table[2*j + 1][i];

		}
		cout << endl;
	}
}


int main(int argc, char* argv[])
{
	// The exit code of the sample application.
	int exitCode = 0;

	Pylon::PylonAutoInitTerm autoInitTerm;

	try
	{

		CDeviceInfo info;
		info.SetDeviceClass(Camera_t::DeviceClass());

		// Get the transport layer factory.
		CTlFactory& tlFactory = CTlFactory::GetInstance();

		// Get all attached devices and exit application if no device is found.
		DeviceInfoList_t devices;
		if (tlFactory.EnumerateDevices(devices) == 0)
		{
			throw RUNTIME_EXCEPTION("No camera present.");
		}
		CBaslerUsbInstantCameraArray cameras(min(devices.size(), c_maxCamerasToUse));

		int CamNmbr = devices.size();

		// Create and attach all Pylon Devices.
		for (size_t i = 0; i < CamNmbr; ++i)
		{
			cameras[i].Attach(tlFactory.CreateDevice(devices[i]));

			//cameras[i].RegisterConfiguration(new CSoftwareTriggerConfiguration, RegistrationMode_ReplaceAll, Cleanup_Delete);
			//cameras[i].RegisterConfiguration(new CConfigurationEventPrinter, RegistrationMode_Append, Cleanup_Delete);

			cameras[i].RegisterImageEventHandler(new CSampleImageEventHandler, RegistrationMode_ReplaceAll, Cleanup_Delete);

			cameras[i].MaxNumBuffer = c_countOfImagesToGrab;

			cameras[i].Open();

			//cameras[i].PixelFormat.SetValue(PixelFormat_Mono12);
			cameras[i].AcquisitionMode.SetValue(AcquisitionMode_Continuous);

			cameras[i].TriggerSelector.SetValue(TriggerSelector_FrameBurstStart);
			cameras[i].TriggerMode.SetValue(TriggerMode_On);
			cameras[i].TriggerSource.SetValue(TriggerSource_Software);
			cameras[i].AcquisitionBurstFrameCount.SetValue(c_countOfImagesToGrab);

			//cameras[i].AcquisitionMode.SetValue(AcquisitionMode_Continuous);
			//cameras[i].AcquisitionBurstFrameCount.SetValue(c_countOfImagesToGrab);


			cameras[i].Gain.SetValue(0);
			cameras[i].ExposureTime.SetValue(50000);
			// Print the model name of the camera.
			cout << "Using device " << cameras[i].GetDeviceInfo().GetModelName() << endl;


			if (GenApi::IsWritable(cameras[i].ChunkModeActive))
			{
				cameras[i].ChunkModeActive.SetValue(true);
			}
			else
			{
				throw RUNTIME_EXCEPTION("The camera doesn't support chunk features");
			}

			// Enable time stamp chunks.
			cameras[i].ChunkSelector.SetValue(ChunkSelector_Timestamp);
			cameras[i].ChunkEnable.SetValue(true);

		}

		//cameras.StartGrabbing(10, GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
		for (size_t i = 0; i < cameras.GetSize(); ++i)
		{
			//cameras[i].StartGrabbing(c_countOfImagesToGrab, GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
			//cameras[i].StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
			cameras[i].AcquisitionStart.Execute();
		}

		//while (!finished)
		//{
		//	// Execute a trigger software command to apply a software acquisition
		//	// start trigger signal to the camera
		//	camera.TriggerSoftware.Execute();
		//	// Perform the required functions to parameterize the frame start
		//	// trigger, to trigger 5 frame starts, and to retrieve 5 frames here
		//}
		//camera.AcquisitionStop.Execute();

		cerr << endl << "Enter \"t\" to trigger the camera or \"e\" to exit and press enter? (t/e)" << endl << endl;

		char key;
		cin.get(key);

		if ((key == 't' || key == 'T'))
		{
			for (size_t i = 0; i < cameras.GetSize(); ++i)
			{
				// Execute the software trigger. Wait up to 100 ms for the camera to be ready for trigger.
				//while(1)
				//{
					cameras[i].TriggerSoftware.Execute();
				//}
			}

			for (size_t i = 0; i < cameras.GetSize(); ++i)
			{
				cameras[i].TriggerSelector.SetValue(TriggerSelector_FrameStart);
				cameras[i].TriggerMode.SetValue(TriggerMode_On);
				cameras[i].TriggerSource.SetValue(TriggerSource_Software);
			}
			for (size_t i = 0; i < cameras.GetSize(); ++i)
			{
				// Execute the software trigger. Wait up to 100 ms for the camera to be ready for trigger.
				if (cameras[i].WaitForFrameTriggerReady(100, TimeoutHandling_ThrowException))
				{
					_PC_frame_start[i] = (double)clock() / CLOCKS_PER_SEC;
					cameras[i].TriggerSoftware.Execute();
					c_FrameSetTriggered++;
				}
			}



			//IsBurstStarted = 1;
		}








		do
		{
			
			cin.get(key);
			
			if ((key == 't' || key == 'T'))
			{
				for (size_t i = 0; i < cameras.GetSize(); ++i)
				{
					// Execute the software trigger. Wait up to 100 ms for the camera to be ready for trigger.
					if (cameras[i].WaitForFrameTriggerReady(5000, TimeoutHandling_ThrowException))
					{
						_PC_frame_start[i] = (double)clock() / CLOCKS_PER_SEC;
						cameras[i].ExecuteSoftwareTrigger();
						//c_FrameSetTriggered++;
					}
				}
				//IsBurstStarted = 1;
			}
				
			




		} while (((key != 'e') && (key != 'E')) );



	}
	catch (GenICam::GenericException &e)
	{
		// Error handling
		cerr << "An exception occurred." << endl
			<< e.GetDescription() << endl;
		exitCode = 1;
	}

	PrintTimeTable();

	// Comment the following two lines to disable waiting on exit.
	cerr << endl << "Press Enter to exit." << endl;
	while (cin.get() != '\n');

	return exitCode;
}