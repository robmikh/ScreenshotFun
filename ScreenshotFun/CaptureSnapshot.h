#pragma once

class CaptureSnapshot 
{
public:
    static winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface Take(
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device,
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item);

private:
    CaptureSnapshot() = delete;
};