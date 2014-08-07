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


		if (IsReadable(ptrGrabResult->ChunkTimestamp))
			_CurrTimestamp[cameraContextValue] = ptrGrabResult->ChunkTimestamp.GetValue();

		//cout << "Camera " << cameraContextValue << ": " << camera.GetDeviceInfo().GetModelName() << (_CurrTimestamp[cameraContextValue] - _PrevTimestamp[cameraContextValue]) << endl;

		cout << "Camera " << cameraContextValue << ": " << camera.GetDeviceInfo().GetModelName() << "fstart: " << _PC_frame_start[cameraContextValue] << "fstop: "<<_PC_frame_stop[cameraContextValue]<< endl;


		_PrevTimestamp[cameraContextValue] = _CurrTimestamp[cameraContextValue];
		
	}
};


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
        if ( tlFactory.EnumerateDevices(devices) == 0 )
        {
            throw RUNTIME_EXCEPTION( "No camera present.");
        }
		CBaslerUsbInstantCameraArray cameras(min(devices.size(), c_maxCamerasToUse));

        // Create and attach all Pylon Devices.
        for ( size_t i = 0; i < cameras.GetSize(); ++i)
        {
            cameras[ i ].Attach( tlFactory.CreateDevice( devices[ i ]));
			
			cameras[i].RegisterConfiguration(new CSoftwareTriggerConfiguration, RegistrationMode_ReplaceAll, Cleanup_Delete);
			cameras[i].RegisterConfiguration(new CConfigurationEventPrinter, RegistrationMode_Append, Cleanup_Delete);

			//cameras[i].RegisterImageEventHandler(new CImageEventPrinter, RegistrationMode_Append, Cleanup_Delete);
			cameras[i].RegisterImageEventHandler(new CSampleImageEventHandler, RegistrationMode_Append, Cleanup_Delete);

			cameras[i].Open();

			//cameras[i].PixelFormat.SetValue(PixelFormat_Mono12);

			cameras[i].Gain.SetValue(0);
			cameras[i].ExposureTime.SetValue(50000);
            // Print the model name of the camera.
            cout << "Using device " << cameras[ i ].GetDeviceInfo().GetModelName() << endl;


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

        // Starts grabbing for all cameras starting with index 0. The grabbing
        // is started for one camera after the other. That's why the images of all
        // cameras are not taken at the same time.
        // However, a hardware trigger setup can be used to cause all cameras to grab images synchronously.
        // According to their default configuration, the cameras are
        // set up for free-running continuous acquisition.
		cameras.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);

        // This smart pointer will receive the grab result data.
		//CBaslerUsbGrabResultPtr ptrGrabResult;
		//camera.StartGrabbing(GrabStrategy_OneByOne, GrabLoop_ProvidedByInstantCamera);

		cerr << endl << "Enter \"t\" to trigger the camera or \"e\" to exit and press enter? (t/e)" << endl << endl;

		// Wait for user input to trigger the camera or exit the program.
		// The grabbing is stopped, the device is closed and destroyed automatically when the camera object goes out of scope.
		char key;
		do
		{
			cin.get(key);
			if ((key == 't' || key == 'T'))
			{
				for (size_t i = 0; i < cameras.GetSize(); ++i)
				{
					// Execute the software trigger. Wait up to 100 ms for the camera to be ready for trigger.
					if (cameras[i].WaitForFrameTriggerReady(100, TimeoutHandling_ThrowException))
					{
						_PC_frame_start[i] = (double)clock() / CLOCKS_PER_SEC;
						cameras[i].ExecuteSoftwareTrigger();
					}
				}
			}
		} while ((key != 'e') && (key != 'E'));


        // Grab c_countOfImagesToGrab from the cameras.
		/*
		for( int i = 0; i < c_countOfImagesToGrab && cameras.IsGrabbing(); ++i)
        {
            cameras.RetrieveResult( 5000, ptrGrabResult, TimeoutHandling_ThrowException);
            intptr_t cameraContextValue = ptrGrabResult->GetCameraContext();
            //Pylon::DisplayImage(cameraContextValue, ptrGrabResult);

			if (PayloadType_ChunkData != ptrGrabResult->GetPayloadType()) throw RUNTIME_EXCEPTION("Unexpected payload type received.");
			

			if (IsReadable(ptrGrabResult->ChunkTimestamp))
				_CurrTimestamp[cameraContextValue] = ptrGrabResult->ChunkTimestamp.GetValue();
			

			cout << "Camera " << cameraContextValue << ": " << cameras[cameraContextValue].GetDeviceInfo().GetModelName() << (_CurrTimestamp[cameraContextValue] - _PrevTimestamp[cameraContextValue]) << endl;
			_PrevTimestamp[cameraContextValue] = _CurrTimestamp[cameraContextValue];
			
		}
		*/

    }
    catch (GenICam::GenericException &e)
    {
        // Error handling
        cerr << "An exception occurred." << endl
        << e.GetDescription() << endl;
        exitCode = 1;
    }

    // Comment the following two lines to disable waiting on exit.
    cerr << endl << "Press Enter to exit." << endl;
    while( cin.get() != '\n');

    return exitCode;
}
