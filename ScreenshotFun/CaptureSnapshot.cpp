#include "pch.h"
#include "CaptureSnapshot.h"

namespace winrt
{
    using namespace Windows;
    using namespace Windows::Foundation;
    using namespace Windows::System;
    using namespace Windows::Graphics;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::DirectX::Direct3D11;
    using namespace Windows::Foundation::Numerics;
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
}

namespace util
{
    using namespace robmikh::common::uwp;
    using namespace robmikh::common::desktop;
}

winrt::IDirect3DSurface
CaptureSnapshot::Take(winrt::IDirect3DDevice const& device, winrt::GraphicsCaptureItem const& item)
{
    auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(device);
    winrt::com_ptr<ID3D11DeviceContext> d3dContext;
    d3dDevice->GetImmediateContext(d3dContext.put());

    // Creating our frame pool with CreateFreeThreaded means that we 
    // will be called back from the frame pool's internal worker thread
    // instead of the thread we are currently on. It also disables the
    // DispatcherQueue requirement.
    auto framePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
        device,
        winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        1,
        item.Size());
    auto session = framePool.CreateCaptureSession(item);

    winrt::IDirect3DSurface result{ nullptr };
    wil::shared_event captureEvent(wil::EventOptions::ManualReset);
    framePool.FrameArrived([session, d3dDevice, d3dContext, &result, captureEvent](auto& framePool, auto&)
    {
        auto frame = framePool.TryGetNextFrame();
        auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

        // Make a copy of the texture
        D3D11_TEXTURE2D_DESC desc = {};
        frameTexture->GetDesc(&desc);
        // Clear flags that we don't need
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        winrt::com_ptr<ID3D11Texture2D> textureCopy;
        winrt::check_hresult(d3dDevice->CreateTexture2D(&desc, nullptr, textureCopy.put()));
        d3dContext->CopyResource(textureCopy.get(), frameTexture.get());
        
        auto dxgiSurface = textureCopy.as<IDXGISurface>();
        result = CreateDirect3DSurface(dxgiSurface.get());

        // End the capture
        session.Close();
        framePool.Close();

        // Signal that we're done
        captureEvent.SetEvent();
    });

    session.StartCapture();

    // Don't return until the capture is finished
    captureEvent.wait();
    WINRT_ASSERT(result != nullptr);

    return result;
}
