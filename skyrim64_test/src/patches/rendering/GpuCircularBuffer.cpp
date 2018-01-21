#include "GpuCircularBuffer.h"

void GpuCircularBuffer::Initialize(ID3D11Device *Device, uint32_t Type, uint32_t BufferSize, uint32_t MaxFrames)
{
	memset(&Map, 0, sizeof(Map));
	memset(&Description, 0, sizeof(Description));

	// Request GPU-side allocation
	Description.ByteWidth = BufferSize;
	Description.Usage = D3D11_USAGE_DYNAMIC;
	Description.BindFlags = Type;
	Description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Description.MiscFlags = 0;
	Description.StructureByteStride = 0;

	if (FAILED(Device->CreateBuffer(&Description, nullptr, &D3DBuffer)))
		__debugbreak();

	FrameUtilizedAmounts = new uint32_t[MaxFrames];
	CurrentAvailable = BufferSize;
}

void *GpuCircularBuffer::MapData(ID3D11DeviceContext *Context, uint32_t AllocationSize, uint32_t *AllocationOffset, bool ForceRemap)
{
	// GPU allocations are required to be 16-byte aligned (explicitly set by me)
	if (AllocationSize % 16 != 0)
		__debugbreak();

	//
	// Allocation boundaries are linear - they don't wrap around to the beginning:
	//
	// <START> ||Free   ||F1 In Use||F2 In Use||F3 In Use||Free     || <END>
	//
	if (CurrentOffset + AllocationSize >= Description.ByteWidth)
	{
		// We exceeded <END> so let's try an allocation from <START>
		CurrentUtilized += Description.ByteWidth - CurrentOffset;
		CurrentOffset = 0;
	}

	uint32_t newSize = CurrentUtilized + AllocationSize;
	uint32_t allocBase = CurrentOffset;

	// Check that we don't exceed buffer size for this frame
	if (newSize >= CurrentAvailable)
		__debugbreak();

	// Allow the buffer to stay mapped across multiple function calls
	if (ForceRemap || !Map.pData)
	{
		if (FAILED(Context->Map(D3DBuffer, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &Map)))
			__debugbreak();
	}

	CurrentUtilized = newSize;
	CurrentOffset += AllocationSize;

	if (AllocationOffset)
		*AllocationOffset = allocBase;

	return (void *)((uintptr_t)Map.pData + allocBase);
}

void GpuCircularBuffer::UnmapData(ID3D11DeviceContext *Context)
{
	if (!Map.pData)
		return;

	Context->Unmap(D3DBuffer, 0);
	memset(&Map, 0, sizeof(Map));
}

void GpuCircularBuffer::SwapFrame(uint32_t FrameIndex)
{
	FrameUtilizedAmounts[FrameIndex] = CurrentUtilized;
	CurrentAvailable -= CurrentUtilized;
	CurrentUtilized = 0;
}

void GpuCircularBuffer::FreeOldFrame(uint32_t FrameIndex)
{
	CurrentAvailable += FrameUtilizedAmounts[FrameIndex];
}
