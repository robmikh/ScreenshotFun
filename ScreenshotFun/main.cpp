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

    // Create our render target
    D3D11_TEXTURE2D_DESC finalTextureDescription = {};
    finalTextureDescription.Width = 350;
    finalTextureDescription.Height = 350;
    finalTextureDescription.MipLevels = 1;
    finalTextureDescription.ArraySize = 1;
    finalTextureDescription.Format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
    finalTextureDescription.SampleDesc.Count = 1;
    finalTextureDescription.SampleDesc.Quality = 0;
    finalTextureDescription.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
    finalTextureDescription.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET | D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;
    finalTextureDescription.CPUAccessFlags = 0;
    finalTextureDescription.MiscFlags = 0;
    com_ptr<ID3D11Texture2D> finalTexture;
    check_hresult(d3dDevice->CreateTexture2D(&finalTextureDescription, nullptr, finalTexture.put()));
    auto dxgiFinalTexture = finalTexture.as<IDXGISurface>();
    com_ptr<ID2D1Bitmap1> d2dTargetBitmap;
    check_hresult(d2dContext->CreateBitmapFromDxgiSurface(dxgiFinalTexture.get(), nullptr, d2dTargetBitmap.put()));
    // Set the render target as our current target
    d2dContext->SetTarget(d2dTargetBitmap.get());

    // Create the geometry clip
    com_ptr<ID2D1PathGeometry> pathGeometry;
    check_hresult(d2dFactory->CreatePathGeometry(pathGeometry.put()));
    com_ptr<ID2D1GeometrySink> geometrySink;
    check_hresult(pathGeometry->Open(geometrySink.put()));
    geometrySink->SetFillMode(D2D1_FILL_MODE_WINDING);
    geometrySink->BeginFigure(D2D1::Point2F(20, 50), D2D1_FIGURE_BEGIN_FILLED);
    geometrySink->AddLine(D2D1::Point2F(130, 50));
    geometrySink->AddLine(D2D1::Point2F(20, 130));
    geometrySink->AddLine(D2D1::Point2F(80, 0));
    geometrySink->AddLine(D2D1::Point2F(130, 130));
    geometrySink->EndFigure(D2D1_FIGURE_END_CLOSED);
    check_hresult(geometrySink->Close());

    // Draw to the render target
    d2dContext->BeginDraw();
    d2dContext->Clear(D2D1::ColorF(0, 0));
    d2dContext->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), pathGeometry.get()), nullptr);
    d2dContext->DrawBitmap(d2dBitmap.get(), &D2D1::RectF(0, 0, 350, 350), 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &D2D1::RectF(350, 550, 700, 900));
    d2dContext->PopLayer();
    check_hresult(d2dContext->EndDraw());

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
    params.PixelWidth = finalTextureDescription.Width;
    params.PixelHeight = finalTextureDescription.Height;

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
    check_hresult(imageEncoder->WriteFrame(d2dTargetBitmap.get(), wicFrame.get(), &params));
    check_hresult(wicFrame->Commit());
    check_hresult(encoder->Commit());

    printf("Done!\nOpening file...\n");
    Launcher::LaunchFileAsync(file).get();
}
