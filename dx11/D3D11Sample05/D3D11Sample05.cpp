#include "stdafx.h"
#include <trace_error.h>
#include <finally.h>
#include <d3dcompiler.h>

struct XYZBuffer
{
	DirectX::XMFLOAT3 Position;
};

struct ColBuffer
{
	DirectX::XMFLOAT3 Color;
};

struct NeverChanges
{
	DirectX::XMFLOAT4X4 Projection;
};

struct ChangesEveryFrame
{
	DirectX::XMFLOAT4X4 View;
	DirectX::XMFLOAT3 Light;
	FLOAT dummy;
};

struct ChangesEveryObject {
	DirectX::XMFLOAT4X4 World;
};

HRESULT readFile(const wchar_t *filename, std::vector<uint8_t> &buf)
{
	HANDLE file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		HRESULT error = GetLastError();
		std::wostringstream oss;
		oss << L"CreateFileW(\"" << filename << L"\", ...) failed.";
		return TRACE_ERROR(error, oss.str().c_str());
	}
	auto closer = finally([=]{ CloseHandle(file); });

	LARGE_INTEGER size;
	if (FAILED(GetFileSizeEx(file, &size))) {
		return TRACE_ERROR(GetLastError(), L"GetFileSizeEx() failed.");
	}
	buf.resize(size.QuadPart);

	DWORD readSize;
	if (ReadFile(file, buf.data(), static_cast<DWORD>(buf.size()), &readSize, nullptr) == 0) {
		return TRACE_ERROR(GetLastError(), L"ReadFile() failed.");
	}

	return S_OK;
}

class Application
{
public:
	Application();
	~Application();

	HRESULT initialize(HINSTANCE hInstance);
	WPARAM run();

private:
	static LRESULT CALLBACK windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT windowProc(UINT msg, WPARAM wParam, LPARAM lParam);
	bool onIdle();
	HRESULT render();

	LRESULT onDestroy(UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT onSize(UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT onKeyDown(UINT msg, WPARAM wParam, LPARAM lParam);

	HRESULT initializeWindow();
	HRESULT initializeDirect3D();
	HRESULT initializeBackBuffer();
	HRESULT isDeviceRemoved();
	void cleanupDirect3D();

	HINSTANCE m_hInstance;
	HWND m_hWnd;

	LONG m_width;
	LONG m_height;

	static const std::wstring m_title;
	static const std::wstring m_windowClass;

	bool m_stanbyMode;
	std::array<FLOAT, 4> m_clearColor;
	DirectX::XMFLOAT3 m_lightPosition;

	struct NeverChanges m_neverChanges;
	struct ChangesEveryFrame m_changesEveryFrame;
	struct ChangesEveryObject m_changesEveryObject;

	D3D_FEATURE_LEVEL m_featureLevel;
	D3D11_VIEWPORT m_viewPort;
	ATL::CComPtr<IDXGISwapChain> m_swapChain;
	ATL::CComPtr<ID3D11Device> m_device;
	ATL::CComPtr<ID3D11DeviceContext> m_immediateContext;
	ATL::CComPtr<ID3D11RenderTargetView> m_renderTargetView;
	ATL::CComPtr<ID3D11Texture2D> m_depthStencilTexture;
	ATL::CComPtr<ID3D11DepthStencilView> m_depthStencilView;
	ATL::CComPtr<ID3D11Buffer> m_xyzVertBuffer;
	ATL::CComPtr<ID3D11Buffer> m_colorVertBuffer;
	ATL::CComPtr<ID3D11Buffer> m_indexBuffer;
	ATL::CComPtr<ID3D11VertexShader> m_vertexShader;
	ATL::CComPtr<ID3D11GeometryShader> m_geometryShader;
	ATL::CComPtr<ID3D11PixelShader> m_pixelShader;
	ATL::CComPtr<ID3D11Buffer> m_constantBufferNeverChanges;
	ATL::CComPtr<ID3D11Buffer> m_constantBufferChangesEveryFrame;
	ATL::CComPtr<ID3D11Buffer> m_constantBufferChangesEveryObject;
	ATL::CComPtr<ID3D11InputLayout> m_inputLayout;
	ATL::CComPtr<ID3D11BlendState> m_blendState;
	ATL::CComPtr<ID3D11RasterizerState> m_rasterizerState;
	ATL::CComPtr<ID3D11DepthStencilState> m_depthStencilState;


};

const std::wstring Application::m_title { L"Direct3D 11 Sample05" };
const std::wstring Application::m_windowClass { L"D3D11D05" };

Application::Application() :
m_hInstance(nullptr),
m_hWnd(nullptr),
m_width(1280),
m_height(720),
m_stanbyMode(false),
m_neverChanges(),
m_changesEveryFrame(),
m_changesEveryObject(),
m_clearColor({ 0.0f, 0.125f, 0.3f, 1.0f }),
m_lightPosition(3.0f, 3.0f, -3.0f),
m_featureLevel(D3D_FEATURE_LEVEL_9_1),
m_viewPort(),
m_swapChain(),
m_device(),
m_immediateContext(),
m_renderTargetView(),
m_depthStencilTexture(),
m_depthStencilView(),
m_xyzVertBuffer(),
m_colorVertBuffer(),
m_indexBuffer(),
m_vertexShader(),
m_geometryShader(),
m_pixelShader(),
m_inputLayout(),
m_blendState(),
m_rasterizerState()
{
}

Application::~Application()
{
	cleanupDirect3D();
	UnregisterClassW(m_windowClass.c_str(), m_hInstance);
}

HRESULT Application::initialize(HINSTANCE hInstance)
{
	m_hInstance = hInstance;

	HRESULT result;
	result = initializeWindow();
	if (FAILED(result)) {
		return result;
	}

	return initializeDirect3D();
}

HRESULT Application::initializeWindow()
{
	WNDCLASSW wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = Application::windowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = m_hInstance;
	wc.hIcon = nullptr;
	wc.hCursor = LoadCursorW(NULL, IDC_ARROW);;
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_APPWORKSPACE);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = m_windowClass.c_str();

	if (RegisterClassW(&wc) == 0) {
		return TRACE_ERROR(GetLastError(), L"RegisterClassW() failed.");
	}

	RECT rect { 0, 0, m_width, m_height };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, TRUE);

	if (CreateWindowExW(0, m_windowClass.c_str(), m_title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, m_width, m_height, nullptr, nullptr, m_hInstance, this) == nullptr) {
		return TRACE_ERROR(GetLastError(), L"CreateWindowExW() failed.");
	}

	ShowWindow(m_hWnd, SW_SHOWNORMAL);
	UpdateWindow(m_hWnd);

	return S_OK;
}

HRESULT Application::initializeDirect3D()
{
	// ID3D11Device, ID3D11DeviceContext
	DXGI_SWAP_CHAIN_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.BufferCount = 3;
	desc.BufferDesc.Width = m_width;
	desc.BufferDesc.Height = m_height;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferDesc.RefreshRate.Numerator = 60;
	desc.BufferDesc.RefreshRate.Denominator = 1;
	desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
	desc.BufferDesc.Scaling = DXGI_MODE_SCALING_CENTERED;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = m_hWnd;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Windowed = TRUE;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

#if defined(_DEBUG)
	UINT createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#else
	UINT createDeviceFlags = 0;
#endif

	std::array<D3D_FEATURE_LEVEL, 3> featureLevels = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

	HRESULT result = S_OK;
	IDXGISwapChain *swapChain = nullptr;
	ID3D11Device *device = nullptr;
	ID3D11DeviceContext *immediateContext = nullptr;

	for (auto driverType : { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE }) {
		result = D3D11CreateDeviceAndSwapChain(
			nullptr,
			driverType,
			nullptr,
			createDeviceFlags,
			featureLevels.data(),
			static_cast<UINT>(featureLevels.size()),
			D3D11_SDK_VERSION,
			&desc,
			&swapChain,
			&device,
			&m_featureLevel,
			&immediateContext);
		if (SUCCEEDED(result)) {
			break;
		}
	}

	if (FAILED(result)) {
		return TRACE_ERROR(result, L"D3D11CreateDeviceAndSwapChain() failed.");
	}

	m_swapChain.Attach(swapChain);
	m_device.Attach(device);
	m_immediateContext.Attach(immediateContext);

	// vertex buffer
	D3D11_BUFFER_DESC xyzBufferDesc;
	xyzBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	xyzBufferDesc.ByteWidth = sizeof(XYZBuffer) * 8;
	xyzBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	xyzBufferDesc.CPUAccessFlags = 0;
	xyzBufferDesc.MiscFlags = 0;
	xyzBufferDesc.StructureByteStride = 0;

	struct XYZBuffer positions[] = {
		DirectX::XMFLOAT3(-1.0f, 1.0f, -1.0f),
		DirectX::XMFLOAT3(1.0f, 1.0f, -1.0f),
		DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f),
		DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f),
		DirectX::XMFLOAT3(-1.0f, 1.0f, 1.0f),
		DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f),
		DirectX::XMFLOAT3(1.0f, -1.0f, 1.0f),
		DirectX::XMFLOAT3(-1.0f, -1.0f, 1.0f)
	};

	D3D11_SUBRESOURCE_DATA xyzSubData;
	xyzSubData.pSysMem = positions;
	xyzSubData.SysMemPitch = 0;
	xyzSubData.SysMemSlicePitch = 0;

	ID3D11Buffer *xyzVertBuffer = nullptr;
	result = m_device->CreateBuffer(&xyzBufferDesc, &xyzSubData, &xyzVertBuffer);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateBuffer() failed.");
	}
	m_xyzVertBuffer.Attach(xyzVertBuffer);

	// color buffer
	D3D11_BUFFER_DESC colorBufferDesc;
	colorBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	colorBufferDesc.ByteWidth = sizeof(ColBuffer) * 8;
	colorBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	colorBufferDesc.CPUAccessFlags = 0;
	colorBufferDesc.MiscFlags = 0;
	colorBufferDesc.StructureByteStride = 0;

	struct ColBuffer colorBuffer[] = {
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f),
		DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
		DirectX::XMFLOAT3(0.0f, 1.0f, 1.0f),
		DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f),
		DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f),
		DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f)
	};

	D3D11_SUBRESOURCE_DATA colorSubData;
	colorSubData.pSysMem = colorBuffer;
	colorSubData.SysMemPitch = 0;
	colorSubData.SysMemSlicePitch = 0;

	ID3D11Buffer *colorVertBuffer = nullptr;
	result = m_device->CreateBuffer(&colorBufferDesc, &colorSubData, &colorVertBuffer);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateBuffer() failed.");
	}
	m_colorVertBuffer.Attach(colorVertBuffer);

	// index buffer
	D3D11_BUFFER_DESC indexBufferDesc;
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(UINT) * 36;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	UINT indexes[] = {
		0, 1, 3,
		1, 2, 3,
		1, 5, 2,
		5, 6, 2,
		5, 4, 6,
		4, 7, 6,
		4, 5, 0,
		5, 1, 0,
		4, 0, 7,
		0, 3, 7,
		3, 2, 7,
		2, 6, 7
	};

	D3D11_SUBRESOURCE_DATA indexSubData;
	indexSubData.pSysMem = indexes;
	indexSubData.SysMemPitch = 0;
	indexSubData.SysMemSlicePitch = 0;

	ID3D11Buffer *indexBuffer = nullptr;
	result = m_device->CreateBuffer(&indexBufferDesc, &indexSubData, &indexBuffer);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateBuffer() failed.");
	}
	m_indexBuffer.Attach(indexBuffer);

	 // vertex shader
	std::vector<uint8_t> vsBlob;
	result = readFile(L"D3D11Sample05Vs.cso", vsBlob);
	if (FAILED(result)) {
		return result;
	}

	ID3D11VertexShader *vertexShader = nullptr;
	result = m_device->CreateVertexShader(vsBlob.data(), vsBlob.size(), nullptr, &vertexShader);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateVertexShader() failed.");
	}
	m_vertexShader.Attach(vertexShader);

	// geometry shader
	std::vector<uint8_t> gsBlob;
	result = readFile(L"D3D11Sample05Gs.cso", gsBlob);
	if (FAILED(result)) {
		return result;
	}

	ID3D11GeometryShader *geometryShader = nullptr;
	result = m_device->CreateGeometryShader(gsBlob.data(), static_cast<DWORD>(gsBlob.size()), nullptr, &geometryShader);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateGeometryShader() failed.");
	}
	m_geometryShader.Attach(geometryShader);

	// pixel shader
	std::vector<uint8_t> psBlob;
	result = readFile(L"D3D11Sample05Ps.cso", psBlob);
	if (FAILED(result)) {
		return result;
	}

	ID3D11PixelShader *pixelShader = nullptr;
	result = m_device->CreatePixelShader(psBlob.data(), static_cast<DWORD>(psBlob.size()), nullptr, &pixelShader);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreatePixelShader() failed.");
	}
	m_pixelShader.Attach(pixelShader);

	// input layout
	std::vector<D3D11_INPUT_ELEMENT_DESC> layout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	ID3D11InputLayout *inputLayout = nullptr;
	result = m_device->CreateInputLayout(layout.data(), layout.size(), vsBlob.data(), vsBlob.size(), &inputLayout);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateInputLayout() failed.");
	}

	// constant buffer
	D3D11_BUFFER_DESC constantBufferDesc;
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantBufferDesc.MiscFlags = 0;

	constantBufferDesc.ByteWidth = sizeof(NeverChanges);
	ID3D11Buffer *constantBufferNeverChanges = nullptr;
	result = m_device->CreateBuffer(&constantBufferDesc, nullptr, &constantBufferNeverChanges);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateBuffer() failed.");
	}
	m_constantBufferNeverChanges.Attach(constantBufferNeverChanges);

	constantBufferDesc.ByteWidth = sizeof(ChangesEveryFrame);
	ID3D11Buffer *constantBufferChangesEveryFrame = nullptr;
	result = m_device->CreateBuffer(&constantBufferDesc, nullptr, &constantBufferChangesEveryFrame);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateBuffer() failed.");
	}
	m_constantBufferChangesEveryFrame.Attach(constantBufferChangesEveryFrame);

	constantBufferDesc.ByteWidth = sizeof(ChangesEveryObject);
	ID3D11Buffer *constantBufferChangesEveryObject = nullptr;
	result = m_device->CreateBuffer(&constantBufferDesc, nullptr, &constantBufferChangesEveryObject);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateBuffer() failed.");
	}
	m_constantBufferChangesEveryObject.Attach(constantBufferChangesEveryObject);

	// blend state
	D3D11_BLEND_DESC blendDesc;
	memset(&blendDesc, 0, sizeof(blendDesc));
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	ID3D11BlendState *blendState = nullptr;
	result = m_device->CreateBlendState(&blendDesc, &blendState);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateBlendState() failed.");
	}
	m_blendState.Attach(blendState);

	// rasterizer state
	D3D11_RASTERIZER_DESC rasterizerDesc;
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.DepthBias = 0;
	rasterizerDesc.DepthBiasClamp = 0;
	rasterizerDesc.SlopeScaledDepthBias = 0;
	rasterizerDesc.DepthClipEnable = FALSE;
	rasterizerDesc.ScissorEnable = FALSE;
	rasterizerDesc.MultisampleEnable = FALSE;
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	ID3D11RasterizerState *rasterizerState = nullptr;
	result = m_device->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateRasterizerState() failed.");
	}
	m_rasterizerState.Attach(rasterizerState);

	// depth stencil state
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	ID3D11DepthStencilState *depthStencilState = nullptr;
	result = m_device->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateDepthStencilState() failed.");
	}
	m_depthStencilState.Attach(depthStencilState);



	result = initializeBackBuffer();
	if (FAILED(result)) {
		return result;
	}


	return S_OK;
}

HRESULT Application::initializeBackBuffer()
{
	HRESULT result = S_OK;
	ID3D11Texture2D *backBuffer = nullptr;
	result = m_swapChain->GetBuffer(0, IID_ID3D11Texture2D, reinterpret_cast<void**>(&backBuffer));
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"IDXGISwapChain::GetBuffer() failed.");
	}
	auto releaser = finally([=]{ backBuffer->Release(); });

	D3D11_TEXTURE2D_DESC backBufferDesc;
	backBuffer->GetDesc(&backBufferDesc);

	ID3D11RenderTargetView *renderTargetView = nullptr;
	result = m_device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateRenderTargetView() failed.");
	}
	m_renderTargetView.Attach(renderTargetView);

	D3D11_TEXTURE2D_DESC depthBufferDesc;
	memset(&depthBufferDesc, 0, sizeof(depthBufferDesc));
	depthBufferDesc.Width = m_width;
	depthBufferDesc.Height = m_height;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.MiscFlags = 0;

	ID3D11Texture2D *depthStencilTexture = nullptr;
	result = m_device->CreateTexture2D(&depthBufferDesc, nullptr, &depthStencilTexture);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateTexture2D() failed.");
	}
	m_depthStencilTexture.Attach(depthStencilTexture);

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvViewDesc;
	memset(&dsvViewDesc, 0, sizeof(dsvViewDesc));
	dsvViewDesc.Format = depthBufferDesc.Format;
	dsvViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvViewDesc.Flags = 0;
	dsvViewDesc.Texture2D.MipSlice = 0;

	ID3D11DepthStencilView *depthStencilView = nullptr;
	result = m_device->CreateDepthStencilView(m_depthStencilTexture, &dsvViewDesc, &depthStencilView);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11Device::CreateDepthStencilView() failed.");
	}
	m_depthStencilView.Attach(depthStencilView);

	m_viewPort.TopLeftX = 0.0f;
	m_viewPort.TopLeftY = 0.0f;
	m_viewPort.Width = static_cast<FLOAT>(m_width);
	m_viewPort.Height = static_cast<FLOAT>(m_height);
	m_viewPort.MinDepth = 0.0f;
	m_viewPort.MaxDepth = 1.0f;

	return S_OK;
}

HRESULT Application::isDeviceRemoved()
{
	HRESULT result = m_device->GetDeviceRemovedReason();
	if (FAILED(result)) {
		TRACE_ERROR(result, L"ID3D11Device::GetDeviceRemovedReason() failed.");
	}

	if (result == DXGI_ERROR_DEVICE_HUNG || result == DXGI_ERROR_DEVICE_RESET) {
		cleanupDirect3D();
		result = initializeDirect3D();
	}

	return result;
}

void Application::cleanupDirect3D()
{
	if (m_immediateContext != nullptr) {
		m_immediateContext->ClearState();
	}
	if (m_swapChain != nullptr) {
		m_swapChain->SetFullscreenState(FALSE, nullptr);
	}
}

bool Application::onIdle()
{
	if (m_device == nullptr) {
		return false;
	}

	if (FAILED(isDeviceRemoved())) {
		return false;
	}

	HRESULT result;

	if (m_stanbyMode) {
		result = m_swapChain->Present(0, DXGI_PRESENT_TEST);
		if (result != S_OK) {
			Sleep(100);
			return true;
		}
		m_stanbyMode = false;
		OutputDebugStringW(L"exit stanby mode\n");
	}

	result = render();
	if (result == DXGI_STATUS_OCCLUDED) {
		m_stanbyMode = true;
		FLOAT c = m_clearColor[0];
		m_clearColor[0] = m_clearColor[1];
		m_clearColor[1] = m_clearColor[2];
		m_clearColor[2] = m_clearColor[3];
		m_clearColor[3] = c;
		OutputDebugStringW(L"enter stanby mode\n");
	}

	return true;
}

HRESULT Application::render()
{
	HRESULT result = S_OK;
	m_immediateContext->ClearRenderTargetView(m_renderTargetView, m_clearColor.data());
	m_immediateContext->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	DirectX::XMVECTORF32 eyePosition = { 0.0f, 5.0f, -5.0f, 1.0f };
	DirectX::XMVECTORF32 focusPosition = { 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMVECTORF32 upDirection = { 0.0f, 1.0f, 0.0f, 1.0f };
	DirectX::XMMATRIX mat = DirectX::XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);
	DirectX::XMStoreFloat4x4(&m_changesEveryFrame.View, DirectX::XMMatrixTranspose(mat));

	DirectX::XMVECTOR vec = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&m_lightPosition), mat);
	DirectX::XMStoreFloat3(&m_changesEveryFrame.Light, vec);

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	result = m_immediateContext->Map(m_constantBufferChangesEveryFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"ID3D11DeviceContext::Map() failed.");
	}
	memcpy(mappedResource.pData, &m_changesEveryFrame, sizeof(ChangesEveryFrame));
	m_immediateContext->Unmap(m_constantBufferChangesEveryFrame, 0);

	ID3D11Buffer *vertBuffers[] = { m_xyzVertBuffer, m_colorVertBuffer };
	UINT strides[] = { sizeof(XYZBuffer), sizeof(ColBuffer) };
	UINT offsets[] = { 0, 0 };
	m_immediateContext->IASetVertexBuffers(0, 2, vertBuffers, strides, offsets);
	m_immediateContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	m_immediateContext->IASetInputLayout(m_inputLayout);
	m_immediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_immediateContext->RSSetViewports(1, &m_viewPort);
	m_immediateContext->RSSetState(m_rasterizerState);
	m_immediateContext->OMSetRenderTargets(1, &m_renderTargetView.p, nullptr);

	FLOAT blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_immediateContext->OMSetBlendState(m_blendState, blendFactor, 0);
	result = m_swapChain->Present(0, 0);
	if (FAILED(result)) {
		return TRACE_ERROR(result, L"IDXGISwapChain::Present() failed.");
	}

	return S_OK;
}

LRESULT CALLBACK Application::windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_NCCREATE) {
		CREATESTRUCTW *cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
		Application *instance = reinterpret_cast<Application*>(cs->lpCreateParams);
		instance->m_hWnd = hWnd;
		SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(instance));
	}

	Application *instance = reinterpret_cast<Application*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
	if (instance != nullptr) {
		return instance->windowProc(msg, wParam, lParam);
	}

	// fallback
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT Application::windowProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_DESTROY:	return onDestroy(msg, wParam, lParam);
	case WM_KEYDOWN:	return onKeyDown(msg, wParam, lParam);
	case WM_SIZE:		return onSize(msg, wParam, lParam);
	default:			return DefWindowProcW(m_hWnd, msg, wParam, lParam);
	}
}

LRESULT Application::onDestroy(UINT msg, WPARAM wParam, LPARAM lParam)
{
	PostQuitMessage(0);
	return 0;
}

LRESULT Application::onKeyDown(UINT msg, WPARAM wParam, LPARAM lParam)
{
	HRESULT result;

	switch (wParam) {
	case VK_ESCAPE:
		PostMessageW(m_hWnd, WM_CLOSE, 0, 0);
		break;
	case VK_F5:
		if (m_swapChain != nullptr) {
			BOOL fullScreen;
			m_swapChain->GetFullscreenState(&fullScreen, nullptr);
			m_swapChain->SetFullscreenState(!fullScreen, nullptr);
		}
		break;
	case VK_F6:
		if (m_swapChain != nullptr) {
			DXGI_MODE_DESC desc;
			desc.Width = 800;
			desc.Height = 600;
			desc.RefreshRate.Numerator = 60;
			desc.RefreshRate.Denominator = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
			desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
			result = m_swapChain->ResizeTarget(&desc);
			if (FAILED(result)) {
				TRACE_ERROR(result, L"IDXGISwapChain::ResizeTarget() failed.");
			}
		}
	default:
		break;
	}

	return DefWindowProcW(m_hWnd, msg, wParam, lParam);
}

LRESULT Application::onSize(UINT msg, WPARAM wParam, LPARAM lParam)
{
	m_width = HIWORD(lParam);
	m_height = LOWORD(lParam);

	if (m_device != nullptr && wParam != SIZE_MINIMIZED) {
		m_immediateContext->OMSetRenderTargets(0, nullptr, nullptr);
		m_renderTargetView.Release();
		m_swapChain->ResizeBuffers(1, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		initializeBackBuffer();
	}

	return DefWindowProcW(m_hWnd, msg, wParam, lParam);
}

WPARAM Application::run()
{
	MSG msg;
	do {
		if (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		} else {
			if (!onIdle()) {
				DestroyWindow(m_hWnd);
			}
		}
	} while (msg.message != WM_QUIT);

	return msg.wParam;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	if (!DirectX::XMVerifyCPUSupport()) {
		return 0;
	}

	Application app;
	if (FAILED(app.initialize(hInstance))) {
		return 0;
	}

	return static_cast<int>(app.run());
}