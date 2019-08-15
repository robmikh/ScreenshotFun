#include "pch.h"
#include "CaptureSnapshot.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::System;

int main()
{
    init_apartment();

	// Get the primary monitor
	auto monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
	auto item = CreateCaptureItemForMonitor(monitor);

	// Get a file to save the screenshot
	auto currentPath = std::filesystem::current_path();
	auto folder = StorageFolder::GetFolderFromPathAsync(currentPath.wstring()).get();
	auto file = folder.CreateFileAsync(L"screenshot.png", CreationCollisionOption::ReplaceExisting).get();

	// Create graphics resources
	auto d3dDevice = CreateD3DDevice();
	auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
	auto device = CreateDirect3DDevice(dxgiDevice.get());

	auto d2dFactory = CreateD2DFactory();
	auto d2dDevice = CreateD2DDevice(d2dFactory, d3dDevice);
	winrt::com_ptr<ID2D1DeviceContext> d2dContext;
	check_hresult(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2dContext.put()));

	// Take a snapshot
	auto asyncOperation = CaptureSnapshot::TakeAsync(device, item);
	auto frame = asyncOperation.get(); // synchronous 
	auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame);
	D3D11_TEXTURE2D_DESC textureDesc = {};
	frameTexture->GetDesc(&textureDesc);
	auto dxgiFrameTexture = frameTexture.as<IDXGISurface>();

	// Get a D2D bitmap for our snapshot
	com_ptr<ID2D1Bitmap1> d2dBitmap;
	check_hresult(d2dContext->CreateBitmapFromDxgiSurface(dxgiFrameTexture.get(), nullptr, d2dBitmap.put()));

	// Get the file stream
	auto randomAccessStream = file.OpenAsync(FileAccessMode::ReadWrite).get();
	auto stream = CreateStreamFromRandomAccessStream(randomAccessStream);

	// Encode the snapshot
	// TODO: dpi?
	auto dpi = 96.0f;
	WICImageParameters params = {};
	params.PixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	params.PixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	params.DpiX = dpi;
	params.DpiY = dpi;
	params.PixelWidth = textureDesc.Width;
	params.PixelHeight = textureDesc.Height;

	auto wicFactory = CreateWICFactory();
	com_ptr<IWICBitmapEncoder> encoder;
	check_hresult(wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()));
	check_hresult(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));

	com_ptr<IWICBitmapFrameEncode> wicFrame;
	com_ptr<IPropertyBag2> frameProperties;
	check_hresult(encoder->CreateNewFrame(wicFrame.put(), frameProperties.put()));
	check_hresult(wicFrame->Initialize(frameProperties.get()));

	com_ptr<IWICImageEncoder> imageEncoder;
	check_hresult(wicFactory->CreateImageEncoder(d2dDevice.get(), imageEncoder.put()));
	check_hresult(imageEncoder->WriteFrame(d2dBitmap.get(), wicFrame.get(), &params));
	check_hresult(wicFrame->Commit());
	check_hresult(encoder->Commit());

    printf("Done!\nOpening file...\n");
	Launcher::LaunchFileAsync(file).get();
}
