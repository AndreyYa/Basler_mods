#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbCamera.h>
#include <ostream>
using namespace Pylon;
using namespace Basler_UsbCameraParams;
using namespace std;

typedef CBaslerUsbCamera Camera_t;

//static const uint32_t c_countOfImagesToGrab = 20;
static const size_t c_maxCamerasToUse = 2;
const int numGrabs = 20;
const int numBuffers = 20;

static vector<int64_t> c_PrevTime(c_maxCamerasToUse, 0);
static vector<int64_t> c_CurrTime(c_maxCamerasToUse, 0);
//int64_t PrevTime = 0;
//int64_t CurrTime = 0;

static vector<Camera_t*> c_CamArray(c_maxCamerasToUse,0);
static vector<CBaslerUsbCamera::StreamGrabber_t*> c_GrabberArray(c_maxCamerasToUse, 0);
static vector<GrabResult*> c_GrabResArray(c_maxCamerasToUse, 0);
static vector<IChunkParser*> c_ChunkParserArray(c_maxCamerasToUse, 0);


static vector<vector<unsigned char*>> c_BuffersArray(c_maxCamerasToUse, vector<unsigned char*>(numBuffers, 0));
//unsigned char* ppBuffers[numBuffers];
static vector<vector<StreamBufferHandle>> c_BufferHandlesArray(c_maxCamerasToUse, vector<StreamBufferHandle>(numBuffers, 0));
//StreamBufferHandle handles[numBuffers];

struct MyContext
{
	// Define some application specific context information here
};
static vector<vector<MyContext*>> c_MyContextArray(c_maxCamerasToUse, vector<MyContext*>(numBuffers, 0));
//MyContext context[numBuffers];



void ProcessImage(unsigned char* pImage, int imageSizeX, int imageSizeY)
{
	//cout << "Camera: x" << imageSizeX << " Y: " << imageSizeY << endl;

	// Do something with the image data
}


int main()
{
	PylonAutoInitTerm autoInitTerm;
	//const int numGrabs = 30;

	try
	{
		// Enumerate	cameras
		CTlFactory& TlFactory = CTlFactory::GetInstance();
		ITransportLayer *pTl = TlFactory.CreateTl(Camera_t::DeviceClass());
		DeviceInfoList_t devices;
		if (0 == pTl->EnumerateDevices(devices)) {
			cerr << "No camera present!" << endl;
			return 1;
		}

		// Create a camera object
		for (int i = 0; i < devices.size(); i++)
		{
			c_CamArray[i] = new Camera_t(pTl->CreateDevice(devices[i]));
			c_GrabResArray[i] = new GrabResult();
		}
		


		for (int i = 0; i < c_CamArray.size(); i++)
		{
			c_CamArray[i]->Open();
			c_CamArray[i]->PixelFormat.SetValue(PixelFormat_Mono8);
			// Maximized AOI
			c_CamArray[i]->OffsetX.SetValue(0);
			c_CamArray[i]->OffsetY.SetValue(0);
			c_CamArray[i]->Width.SetValue(c_CamArray[i]->Width.GetMax());
			c_CamArray[i]->Height.SetValue(c_CamArray[i]->Height.GetMax());
			// Continuous mode, no external trigger used
			c_CamArray[i]->TriggerSelector.SetValue(TriggerSelector_FrameBurstStart);
			c_CamArray[i]->TriggerMode.SetValue(TriggerMode_On);
			c_CamArray[i]->TriggerSource.SetValue(TriggerSource_Software);

			c_CamArray[i]->AcquisitionMode.SetValue(AcquisitionMode_Continuous);
			c_CamArray[i]->AcquisitionBurstFrameCount.SetValue(numGrabs);
			// Configure exposure time and mode
			c_CamArray[i]->ExposureMode.SetValue(ExposureMode_Timed);
			c_CamArray[i]->ExposureTime.SetValue(100);

			if (GenApi::IsWritable(c_CamArray[i]->ChunkModeActive)) {
				c_CamArray[i]->ChunkModeActive.SetValue(true);
			}
			else 
			{
				cerr << "The camera does not support chunk features" << endl;
				return 1;
			}

			c_CamArray[i]->ChunkSelector.SetValue(ChunkSelector_Timestamp);
			c_CamArray[i]->ChunkEnable.SetValue(true);
			c_ChunkParserArray[i] = c_CamArray[i]->CreateChunkParser();
		}


		for (int i = 0; i < c_CamArray.size(); i++)
		{
			// check whether stream grabbers are avalaible
			if (c_CamArray[i]->GetNumStreamGrabberChannels() == 0) {
				cerr << "Camera doesn't support stream grabbers." << endl;
			}
			else
			{
				CBaslerUsbCamera::StreamGrabber_t* pGrabber = new CBaslerUsbCamera::StreamGrabber_t(c_CamArray[i]->GetStreamGrabber(0));
				pGrabber->Open();
				c_GrabberArray[i] = pGrabber;

				// Parameterize the stream grabber
				const int bufferSize = (int)c_CamArray[i]->PayloadSize();
				c_GrabberArray[i]->MaxBufferSize = bufferSize;
				c_GrabberArray[i]->MaxNumBuffer = numBuffers;
				c_GrabberArray[i]->PrepareGrab();

				for (int j = 0; j < numBuffers; ++j)
				{					
					c_BuffersArray[i][j] = new unsigned char[bufferSize];
					c_BufferHandlesArray[i][j] = c_GrabberArray[i]->RegisterBuffer((void*)c_BuffersArray[i][j], bufferSize);
					c_GrabberArray[i]->QueueBuffer(c_BufferHandlesArray[i][j], c_MyContextArray[i][j]);
				}
			}
		}
		
		for (int i = 0; i < c_CamArray.size(); i++)
		{
			// Start image acquisition
			c_CamArray[i]->AcquisitionStart.Execute();
		}

		

		for (int i = 0; i < c_CamArray.size(); i++)
		{
			// Start image acquisition
			c_CamArray[i]->TriggerSoftware.Execute();
		}

		for (int j = 0; j < numGrabs; ++j) 
		{		
			for (int i = 0; i < c_CamArray.size(); i++)
			{
				if (c_GrabberArray[i]->GetWaitObject().Wait(3000)) 
				{
					// Get an item from the grabber's output queue
					if (!c_GrabberArray[i]->RetrieveResult(*c_GrabResArray[i])) {
						cerr << "Failed to retrieve an item from the output queue" << endl;
						break;
					}

					if (c_GrabResArray[i]->Succeeded()) 
					{
						// Grabbing was successful. Process the image.
						c_ChunkParserArray[i]->AttachBuffer(c_GrabResArray[i]->Buffer(), c_GrabResArray[i]->GetPayloadSize());

						if (IsReadable(c_CamArray[i]->ChunkTimestamp))
						{
							c_CurrTime[i] = c_CamArray[i]->ChunkTimestamp.GetValue();
							cout << "TimeStamp Cam"<< i << ": " << (c_CurrTime[i] * 0.000000001) << " dT:" << ((c_CurrTime[i] - c_PrevTime[i])*0.000000001) << endl;
							c_PrevTime[i] = c_CurrTime[i];
						}
						ProcessImage((unsigned char*)c_GrabResArray[i]->Buffer(), c_GrabResArray[i]->GetSizeX(), c_GrabResArray[i]->GetSizeY());
					}
					else 
					{
						cerr << "Grab failed: " << c_GrabResArray[i]->GetErrorDescription() << endl;
						break;
					}
					// Requeue the buffer
					if (i + numBuffers < numGrabs)
						c_GrabberArray[i]->QueueBuffer(c_GrabResArray[i]->Handle(), c_GrabResArray[i]->Context());
				}
				else 
				{
					cerr << "timeout occurred when waiting for a grabbed image" << endl;
					break;
				}
			}
		}

		// Finished. Stop grabbing and do clean-up
		for (int i = 0; i < c_CamArray.size(); i++)
		{
			// Start image acquisition
			c_CamArray[i]->AcquisitionStop.Execute();

			while (c_GrabberArray[i]->GetWaitObject().Wait(0)) 
			{
				c_GrabberArray[i]->RetrieveResult(*c_GrabResArray[i]);
				if (c_GrabResArray[i]->Status() == Canceled)
					cout << "Got canceled buffer" << endl;
			}

			// Deregister and free buffers
			for (int j = 0; j < numBuffers; ++j) 
			{
				//c_GrabberArray[i]->DeregisterBuffer(handles[j]);
				c_GrabberArray[i]->DeregisterBuffer(c_BufferHandlesArray[i][j]);
					
				//delete[] ppBuffers[j];
				delete[] c_BuffersArray[i][j];
			}

			c_GrabberArray[i]->FinishGrab();
			c_GrabberArray[i]->Close();

			c_CamArray[i]->Close();
		}
			
		TlFactory.ReleaseTl(pTl);
	}
	catch (GenICam::GenericException &e)
	{
		// Error handling
		cerr << "An exception occurred!" << endl << e.GetDescription() << endl;
		return 1;
	}

	// Quit application
	return 0;
}