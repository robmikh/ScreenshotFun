#include "pch.h"
#include "CaptureSnapshot.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX::Direct3D11;
    using namespace Windows::Storage;
    using namespace Windows::System;
}

namespace util
{
    using namespace robmikh::common::uwp;
    using namespace robmikh::common::desktop;
}

winrt::IAsyncAction SaveBitmapToFileAsync(winrt::com_ptr<ID2D1Device> const& d2dDevice, winrt::com_ptr<ID2D1Bitmap1> const& d2dBitmap, winrt::StorageFile const& file);
winrt::com_ptr<ID3D11Texture2D> CreateTexture(winrt::com_ptr<ID3D11Device> const& d3dDevice, uint32_t width, uint32_t height);
winrt::com_ptr<ID2D1PathGeometry> BuildGeometry(winrt::com_ptr<ID2D1Factory1> const& d2dFactory, uint32_t width, uint32_t height);

winrt::IAsyncAction MainAsync()
{
    // Get the primary monitor
    auto monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    auto item = util::CreateCaptureItemForMonitor(monitor);
    auto itemSize = item.Size();

    // Get a file to save the screenshot
    auto currentPath = std::filesystem::current_path();
    auto folder = co_await winrt::StorageFolder::GetFolderFromPathAsync(currentPath.wstring());
    auto file = co_await folder.CreateFileAsync(L"screenshot.png", winrt::CreationCollisionOption::ReplaceExisting);

    // Create graphics resources
    auto d3dDevice = util::CreateD3DDevice();
    auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
    auto device = CreateDirect3DDevice(dxgiDevice.get());

    auto d2dFactory = util::CreateD2DFactory();
    auto d2dDevice = util::CreateD2DDevice(d2dFactory, d3dDevice);
    winrt::com_ptr<ID2D1DeviceContext> d2dContext;
    winrt::check_hresult(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2dContext.put()));

    // Take a snapshot
    auto frame = CaptureSnapshot::Take(device, item);  
    auto frameTexture = GetDXGIInterfaceFromObject<IDXGISurface>(frame);

    // Get a D2D bitmap for our snapshot
    winrt::com_ptr<ID2D1Bitmap1> d2dBitmap;
    winrt::check_hresult(d2dContext->CreateBitmapFromDxgiSurface(frameTexture.get(), nullptr, d2dBitmap.put()));

    // Create our render target
    auto renderTargetWidth = 350;
    auto renderTargetHeight = 350;
    auto finalTexture = CreateTexture(d3dDevice, renderTargetWidth, renderTargetHeight);
    auto dxgiFinalTexture = finalTexture.as<IDXGISurface>();
    winrt::com_ptr<ID2D1Bitmap1> d2dTargetBitmap;
    winrt::check_hresult(d2dContext->CreateBitmapFromDxgiSurface(dxgiFinalTexture.get(), nullptr, d2dTargetBitmap.put()));

    // Set the render target as our current target
    d2dContext->SetTarget(d2dTargetBitmap.get());

    // Create the geometry clip
    auto geometry = BuildGeometry(d2dFactory, renderTargetWidth, renderTargetHeight);

    // Compute the rect we want to copy from the snapshot bitmap
    auto left = (itemSize.Width - renderTargetWidth) / 2.0f;
    auto top = (itemSize.Height - renderTargetHeight) / 2.0f;
    auto sourceRect = D2D1::RectF(left, top, left + renderTargetWidth, top + renderTargetHeight);

    // Draw to the render target
    d2dContext->BeginDraw();
    d2dContext->Clear(D2D1::ColorF(0, 0));
    d2dContext->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), geometry.get()), nullptr);
    d2dContext->DrawBitmap(
        d2dBitmap.get(), 
        D2D1::RectF(0, 0, renderTargetWidth, renderTargetHeight), 
        1.0f, 
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, 
        &sourceRect);
    d2dContext->PopLayer();
    winrt::check_hresult(d2dContext->EndDraw());

    co_await SaveBitmapToFileAsync(d2dDevice, d2dTargetBitmap, file);

    printf("Done!\nOpening file...\n");
    co_await winrt::Launcher::LaunchFileAsync(file);
}

int main()
{
    winrt::init_apartment();
    MainAsync().get(); // synchronous
    return 0;
}

winrt::IAsyncAction SaveBitmapToFileAsync(winrt::com_ptr<ID2D1Device> const& d2dDevice, winrt::com_ptr<ID2D1Bitmap1> const& d2dBitmap, winrt::StorageFile const& file)
{
    // Get the file stream
    auto randomAccessStream = co_await file.OpenAsync(winrt::FileAccessMode::ReadWrite);
    auto stream = util::CreateStreamFromRandomAccessStream(randomAccessStream);

    // Encode the snapshot
    auto wicFactory = util::CreateWICFactory();
    winrt::com_ptr<IWICBitmapEncoder> encoder;
    winrt::check_hresult(wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()));
    winrt::check_hresult(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));

    winrt::com_ptr<IWICBitmapFrameEncode> wicFrame;
    winrt::com_ptr<IPropertyBag2> frameProperties;
    winrt::check_hresult(encoder->CreateNewFrame(wicFrame.put(), frameProperties.put()));
    winrt::check_hresult(wicFrame->Initialize(frameProperties.get()));

    winrt::com_ptr<IWICImageEncoder> imageEncoder;
    winrt::check_hresult(wicFactory->CreateImageEncoder(d2dDevice.get(), imageEncoder.put()));
    winrt::check_hresult(imageEncoder->WriteFrame(d2dBitmap.get(), wicFrame.get(), nullptr));
    winrt::check_hresult(wicFrame->Commit());
    winrt::check_hresult(encoder->Commit());
}

winrt::com_ptr<ID3D11Texture2D> CreateTexture(winrt::com_ptr<ID3D11Device> const& d3dDevice, uint32_t width, uint32_t height)
{
    D3D11_TEXTURE2D_DESC textureDescription = {};
    textureDescription.Width = width;
    textureDescription.Height = height;
    textureDescription.MipLevels = 1;
    textureDescription.ArraySize = 1;
    textureDescription.Format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDescription.SampleDesc.Count = 1;
    textureDescription.SampleDesc.Quality = 0;
    textureDescription.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
    textureDescription.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET | D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;
    textureDescription.CPUAccessFlags = 0;
    textureDescription.MiscFlags = 0;
    winrt::com_ptr<ID3D11Texture2D> texture;
    winrt::check_hresult(d3dDevice->CreateTexture2D(&textureDescription, nullptr, texture.put()));

    return texture;
}

winrt::com_ptr<ID2D1PathGeometry> BuildGeometry(winrt::com_ptr<ID2D1Factory1> const& d2dFactory, uint32_t width, uint32_t height)
{
    // Build the proportions for our star
    auto midHeight = height / 3.0f;
    auto bottomRatio = width / 8.0f;
    auto topRatio = width / 2.0f;

    winrt::com_ptr<ID2D1PathGeometry> pathGeometry;
    winrt::check_hresult(d2dFactory->CreatePathGeometry(pathGeometry.put()));
    winrt::com_ptr<ID2D1GeometrySink> geometrySink;
    winrt::check_hresult(pathGeometry->Open(geometrySink.put()));
    geometrySink->SetFillMode(D2D1_FILL_MODE_WINDING);
    geometrySink->BeginFigure(D2D1::Point2F(0, midHeight), D2D1_FIGURE_BEGIN_FILLED);
    geometrySink->AddLine(D2D1::Point2F(width, midHeight));
    geometrySink->AddLine(D2D1::Point2F(bottomRatio, height));
    geometrySink->AddLine(D2D1::Point2F(topRatio, 0));
    geometrySink->AddLine(D2D1::Point2F(7.0f * bottomRatio, height));
    geometrySink->EndFigure(D2D1_FIGURE_END_CLOSED);
    winrt::check_hresult(geometrySink->Close());

    return pathGeometry;
}
