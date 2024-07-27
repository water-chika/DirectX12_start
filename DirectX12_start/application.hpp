#include <dxgi.h>
#include <type_traits>
#include <vector>
#include <iostream>
#include <d3d12.h>
#include <array>
#include <d3dcompiler.h>

#define uuid_ppv(pv) \
	__uuidof(std::remove_reference_t<decltype(*(pv))>), reinterpret_cast<void**>(&pv)

#define throw_if_fail(exp) \
	if (S_OK != (exp)) { \
		throw std::runtime_error(#exp " fails"); \
	}

class Application {
public:
	inline void init() {
		IDXGIFactory* pFactory{};
		throw_if_fail(CreateDXGIFactory(uuid_ppv(pFactory)));
		std::vector<IDXGIAdapter*> pAdapters{ get_adapters(pFactory) };
		for (auto pAdapter : pAdapters) {
			DXGI_ADAPTER_DESC desc{};
			throw_if_fail(pAdapter->GetDesc(&desc));
			std::wcout << desc.Description << std::endl;
			std::cout << desc.DedicatedVideoMemory << std::endl;
		}
		ID3D12Debug* pDebug{};
		throw_if_fail(D3D12GetDebugInterface(uuid_ppv(pDebug)));
		pDebug->EnableDebugLayer();
		ID3D12Device* pDevice{};
		throw_if_fail(D3D12CreateDevice(pAdapters[0], D3D_FEATURE_LEVEL_12_2, uuid_ppv(pDevice)));
		ID3D12CommandQueue* pCommandQueue{ create_command_queue(pDevice) };
		ID3D12CommandAllocator* pCommandAllocator{ create_command_allocator(pDevice) };
		ID3D12GraphicsCommandList* pCommandList{ create_command_list(pDevice, pCommandAllocator) };
		ID3D12RootSignature* pRootSignature{ create_root_signature(pDevice) };
		ID3D12PipelineState* pPipelineState{ create_pipeline_state(pDevice, pRootSignature) };
		pCommandList->SetComputeRootSignature(pRootSignature);
		pCommandList->SetPipelineState(pPipelineState);
		ID3D12Resource* pBuffer = create_buffer(pDevice);
		//ID3D12DescriptorHeap* pDescriptorHeap = create_descriptor_heap(pDevice);
		//create_unordered_access_view(pDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), pDevice, pBuffer);
		pCommandList->SetComputeRootUnorderedAccessView(0, pBuffer->GetGPUVirtualAddress());
		pCommandList->Dispatch(1, 1, 1);
		pCommandList->Close();

		ID3D12Fence* pFence{};
		throw_if_fail(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, uuid_ppv(pFence)));
		ID3D12CommandList* pList{};
		throw_if_fail(pCommandList->QueryInterface(&pList));
		pCommandQueue->ExecuteCommandLists(1, &pList);
		throw_if_fail(pCommandQueue->Signal(pFence, 1));

		while (0 == pFence->GetCompletedValue()) {

		}
		D3D12_RANGE range{};
		range.Begin = 0;
		range.End = 64;
		int* data{};
		throw_if_fail(pBuffer->Map(0, &range, reinterpret_cast<void**>(&data)));
		std::cout << data[0] << std::endl;
	}
	ID3D12DescriptorHeap* create_descriptor_heap(ID3D12Device* pDevice) {
		ID3D12DescriptorHeap* pHeap{};
		D3D12_DESCRIPTOR_HEAP_DESC desc{};
		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		throw_if_fail(pDevice->CreateDescriptorHeap(&desc, uuid_ppv(pHeap)));
		return pHeap;
	}
	ID3D12Resource* create_buffer(ID3D12Device* pDevice) {
		D3D12_HEAP_PROPERTIES heap_properties{};
		heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
		heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		heap_properties.Type = D3D12_HEAP_TYPE_CUSTOM;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.MipLevels = 1;
		desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		desc.DepthOrArraySize = 1;
		desc.Width = 256;
		desc.Height = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		ID3D12Resource* pBuffer{};
		throw_if_fail(pDevice->CreateCommittedResource(
			&heap_properties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			uuid_ppv(pBuffer)
		));
		return pBuffer;
	}
	void create_unordered_access_view(D3D12_CPU_DESCRIPTOR_HANDLE handle, ID3D12Device* pDevice, ID3D12Resource* pBuffer) {
		D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		desc.Buffer.NumElements = 1;
		desc.Buffer.StructureByteStride = sizeof(uint64_t);
		desc.Buffer.CounterOffsetInBytes = 0;
		pDevice->CreateUnorderedAccessView(
			pBuffer,
			nullptr,
			&desc,
			handle
		);
	}
	ID3DBlob* create_compute_shader() {
		const char* src =
			"RWStructuredBuffer<uint> g_value : register(u);\n"
			"[numthreads(1,1,1)]\n"
			"void CS(uint3 id : SV_DispatchThreadID)\n"
			"{\n"
			"    g_value[0]=1;\n"
			"}";
		ID3DBlob* pCode{}, * pErrorMsgs{};
		throw_if_fail(D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, "CS", "cs_5_0", 0, 0, &pCode, &pErrorMsgs));
		return pCode;
	}
	ID3D12RootSignature* create_root_signature(ID3D12Device* pDevice) {
		ID3D12RootSignature* pRootSignature{};
		ID3DBlob* pBlob{}, * pErrorBlob{};
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
		desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		D3D12_ROOT_SIGNATURE_DESC1 desc1_1{};
		std::array<D3D12_ROOT_PARAMETER1, 1> parameters;
		parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		parameters[0].Descriptor = D3D12_ROOT_DESCRIPTOR1{0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE};
		parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		desc1_1.NumParameters = 1;
		desc1_1.pParameters = parameters.data();
		desc.Desc_1_1 = desc1_1;
		throw_if_fail(D3D12SerializeVersionedRootSignature(&desc, &pBlob, &pErrorBlob));
		throw_if_fail(pDevice->CreateRootSignature(0, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), uuid_ppv(pRootSignature)));
		return pRootSignature;
	}
	ID3D12PipelineState* create_pipeline_state(ID3D12Device* pDevice, ID3D12RootSignature* pRootSignature) {
		ID3D12PipelineState* pPipelineState{};
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = pRootSignature;
		ID3DBlob* pCode = create_compute_shader();
		desc.CS = D3D12_SHADER_BYTECODE{pCode->GetBufferPointer(), pCode->GetBufferSize()};
		throw_if_fail(pDevice->CreateComputePipelineState(&desc, uuid_ppv(pPipelineState)));
		return pPipelineState;
	}
	ID3D12GraphicsCommandList* create_command_list(ID3D12Device* pDevice, ID3D12CommandAllocator* pAllocator) {
		ID3D12GraphicsCommandList* pCommandList{};
		throw_if_fail(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAllocator, nullptr, uuid_ppv(pCommandList)));
		return pCommandList;
	}
	ID3D12CommandAllocator* create_command_allocator(ID3D12Device* pDevice) {
		ID3D12CommandAllocator* pCommandAllocator{};
		throw_if_fail(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, uuid_ppv(pCommandAllocator)));
		return pCommandAllocator;
	}
	ID3D12CommandQueue* create_command_queue(ID3D12Device* pDevice) {
		D3D12_COMMAND_QUEUE_DESC desc{};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
		desc.NodeMask = 0;
		ID3D12CommandQueue* pCommandQueue;
		throw_if_fail(pDevice->CreateCommandQueue(&desc, uuid_ppv(pCommandQueue)));
		return pCommandQueue;
	}
	std::vector<IDXGIAdapter*> get_adapters(IDXGIFactory* pFactory) {
		std::vector<IDXGIAdapter*> pAdapters{};
		IDXGIAdapter* pAdapter{};
		for (UINT i = 0; S_OK == pFactory->EnumAdapters(i, &pAdapter); i++) {
			pAdapters.emplace_back(pAdapter);
		}
		return pAdapters;
	}
private:
};