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
static const size_t c_maxCamerasToUse = 2;

static vector<int64_t> _CurrTimestamp(c_maxCamerasToUse, 0);
static vector<int64_t> _PrevTimestamp(c_maxCamerasToUse, 0);

static vector<double> _PC_frame_start(c_maxCamerasToUse, 0.0);
static vector<double> _PC_frame_stop(c_maxCamerasToUse, 0.0);
static vector<int> _PC_frame_count(c_maxCamerasToUse, 0);

static vector<vector<double>> _PC_frame_time_table(c_maxCamerasToUse*2, vector<double>(c_countOfImagesToGrab + 3, 0.0));

static char IsBurstStarted = 0;
static int c_FrameSetTriggered = -1;

class CSampleImageEventHandler : public CBaslerUsbImageEventHandler //CImageEventHandler //CBaslerUsbImageEventHandler
{
public:
	virtual void OnImageGrabbed(CBaslerUsbInstantCamera& camera, const CBaslerUsbGrabResultPtr& ptrGrabResult)
		//virtual void OnImageGrabbed(CInstantCamera& camera, const CGrabResultPtr& ptrGrabResult)
	{
		intptr_t cameraContextValue = ptrGrabResult->GetCameraContext();
		_PC_frame_stop[cameraContextValue] = (double)clock() / CLOCKS_PER_SEC;
		Pylon::DisplayImage(cameraContextValue, ptrGrabResult);

		if (PayloadType_ChunkData != ptrGrabResult->GetPayloadType()) throw RUNTIME_EXCEPTION("Unexpected payload type received.");


		if (_PC_frame_count[cameraContextValue] < c_countOfImagesToGrab)
		{
			//if (IsReadable(ptrGrabResult->ChunkTimestamp))
			//{
			int _frame_index = _PC_frame_count[cameraContextValue];
			_PC_frame_time_table[cameraContextValue][_frame_index] = _PC_frame_stop[cameraContextValue];// (double)clock() / CLOCKS_PER_SEC;
			
			if (IsReadable(ptrGrabResult->ChunkTimestamp))
			{
				_PC_frame_time_table[cameraContextValue+2][_frame_index] = 0.000000001*(double)ptrGrabResult->ChunkTimestamp.GetValue();
			}
			//cout << "Camera " << cameraContextValue << ": " << _PC_frame_stop[cameraContextValue] << " frmNm: " << _PC_frame_count[cameraContextValue] << " frmTime: " << _PC_frame_time_table[cameraContextValue][_PC_frame_count[cameraContextValue]] << endl;

			_PC_frame_count[cameraContextValue] += 1;


			_PrevTimestamp[cameraContextValue] = _CurrTimestamp[cameraContextValue];

			camera.ExecuteSoftwareTrigger();
			_PC_frame_start[cameraContextValue] = (double)clock() / CLOCKS_PER_SEC;

		}


		//		cout << "Camera " << cameraContextValue << ": " << _PC_frame_stop[cameraContextValue] << " frmNm: " << _PC_frame_count[cameraContextValue] << " frmTime: " << _PC_frame_time_table[cameraContextValue][_PC_frame_count[cameraContextValue]] << endl;


		//cout << "Camera " << cameraContextValue << ": " << camera.GetDeviceInfo().GetModelName() << (_CurrTimestamp[cameraContextValue] - _PrevTimestamp[cameraContextValue]) << endl;

		//cout << "Camera " << cameraContextValue << ": " << camera.GetDeviceInfo().GetModelName() << "fstart: " << _PC_frame_start[cameraContextValue] << "fstop: "<<_PC_frame_stop[cameraContextValue]<< endl;



	}
};

void PrintTimeTable()
{
	for (size_t i = 0; i < c_countOfImagesToGrab; ++i)
	{
		cout << "Camera";
		//for (size_t j = 0; j < c_maxCamerasToUse; ++j)
		for (size_t j = 0; j < c_maxCamerasToUse; ++j)
		{
			//cout << " #" << j << ": " << _PC_frame_time_table[j][i] << " Cam Time:" << _PC_frame_time_table[j+2][i];
			cout << " #" << j << ": " << _PC_frame_time_table[j][i + 1] - _PC_frame_time_table[j][i] << " Cam Time:" << _PC_frame_time_table[j + 2][i + 1] - _PC_frame_time_table[j + 2][i];

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

		// Create and attach all Pylon Devices.
		for (size_t i = 0; i < cameras.GetSize(); ++i)
		{
			cameras[i].Attach(tlFactory.CreateDevice(devices[i]));

			cameras[i].RegisterConfiguration(new CSoftwareTriggerConfiguration, RegistrationMode_ReplaceAll, Cleanup_Delete);
			cameras[i].RegisterConfiguration(new CConfigurationEventPrinter, RegistrationMode_Append, Cleanup_Delete);

			//cameras[i].RegisterImageEventHandler(new CImageEventPrinter, RegistrationMode_Append, Cleanup_Delete);
			cameras[i].RegisterImageEventHandler(new CSampleImageEventHandler, RegistrationMode_Append, Cleanup_Delete);

			cameras[i].Open();

			//cameras[i].PixelFormat.SetValue(PixelFormat_Mono12);

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
			cameras[i].StartGrabbing(c_countOfImagesToGrab + 2, GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);
		}


		cerr << endl << "Enter \"t\" to trigger the camera or \"e\" to exit and press enter? (t/e)" << endl << endl;

		char key;



		do
		{
			if (IsBurstStarted == 0)
			{
				cin.get(key);
			}
			else key = 't';

				if ((key == 't' || key == 'T'))
				{
					for (size_t i = 0; i < cameras.GetSize(); ++i)
					{
						// Execute the software trigger. Wait up to 100 ms for the camera to be ready for trigger.
						if (cameras[i].WaitForFrameTriggerReady(300, TimeoutHandling_ThrowException))
						{
							_PC_frame_start[i] = (double)clock() / CLOCKS_PER_SEC;
							cameras[i].ExecuteSoftwareTrigger();
							c_FrameSetTriggered++;
						}
					}
					IsBurstStarted = 1;
				}
				
			
			//else
			{
				/*for (size_t i = 0; i < cameras.GetSize(); ++i)
				{
					//cout << "Trigger run " << endl;
					_PC_frame_start[i] = (double)clock() / CLOCKS_PER_SEC;
					cameras[i].ExecuteSoftwareTrigger();
					c_FrameSetTriggered++;
				}*/
			}




		} while (((key != 'e') && (key != 'E')) || (c_FrameSetTriggered < c_countOfImagesToGrab));



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