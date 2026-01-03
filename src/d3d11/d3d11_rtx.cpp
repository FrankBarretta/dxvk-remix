#include "d3d11_rtx.h"
#include "d3d11_device.h"
#include "d3d11_context.h"
#include "d3d11_buffer.h"
#include "d3d11_input_layout.h"
#include "../dxvk/rtx_render/rtx_context.h"

#include <algorithm>
#include <vector>

namespace dxvk {
  
  // Helper to read buffer data
  static std::vector<uint8_t> ReadBuffer(
    D3D11DeviceContext* ctx,
    ID3D11Buffer*       buffer,
    UINT                offset,
    UINT                size) {
      
    if (!buffer || size == 0) return {};

    D3D11_BUFFER_DESC desc;
    buffer->GetDesc(&desc);

    D3D11_BUFFER_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;
    stagingDesc.ByteWidth = size;

    Com<ID3D11Buffer> stagingBuffer;
    ID3D11Device* pDevice = nullptr;
    ctx->GetDevice(&pDevice);
    HRESULT hr = pDevice->CreateBuffer(&stagingDesc, nullptr, &stagingBuffer);
    pDevice->Release();
    if (FAILED(hr)) {
      Logger::err("D3D11Rtx: Failed to create staging buffer");
      return {};
    }

    D3D11_BOX box;
    box.left = offset;
    box.right = offset + size;
    box.top = 0;
    box.bottom = 1;
    box.front = 0;
    box.back = 1;

    ctx->CopySubresourceRegion(stagingBuffer.ptr(), 0, 0, 0, 0, buffer, 0, &box);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = ctx->Map(stagingBuffer.ptr(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
      Logger::err("D3D11Rtx: Failed to map staging buffer");
      return {};
    }

    std::vector<uint8_t> data(size);
    std::memcpy(data.data(), mapped.pData, size);

    ctx->Unmap(stagingBuffer.ptr(), 0);

    return data;
  }

  D3D11Rtx::D3D11Rtx(D3D11Device* d3d11Device)
    : m_parent(d3d11Device) {
  }


  D3D11Rtx::~D3D11Rtx() {
  }

  void D3D11Rtx::Draw(
    D3D11DeviceContext* pContext,
    UINT VertexCount,
    UINT StartVertexLocation) {
      // Placeholder: Implement RTX logic here
  }

  void D3D11Rtx::DrawIndexed(
    D3D11DeviceContext* pContext,
    UINT IndexCount,
    UINT StartIndexLocation,
    INT  BaseVertexLocation) {
      
      const auto& state = pContext->m_state;
      if (state.ia.inputLayout.ptr() == nullptr) return;

      // Find POSITION attribute
      const auto& elements = state.ia.inputLayout->GetElementDescs();
      const auto& attributes = state.ia.inputLayout->GetAttributes();
      
      int positionIdx = -1;
      for (size_t i = 0; i < elements.size(); i++) {
        if (elements[i].SemanticName == "POSITION" && elements[i].SemanticIndex == 0) {
          positionIdx = i;
          break;
        }
      }

      if (positionIdx == -1) return;

      const auto& attrib = attributes[positionIdx];
      const auto& binding = state.ia.vertexBuffers[attrib.binding];
      
      if (binding.buffer.ptr() == nullptr) return;

      // Read Index Buffer
      if (state.ia.indexBuffer.buffer.ptr() == nullptr) return;
      
      UINT indexSize = state.ia.indexBuffer.format == DXGI_FORMAT_R32_UINT ? 4 : 2;
      UINT indexOffset = state.ia.indexBuffer.offset + StartIndexLocation * indexSize;
      UINT indexDataSize = IndexCount * indexSize;
      
      std::vector<uint8_t> indexData = ReadBuffer(pContext, state.ia.indexBuffer.buffer.ptr(), indexOffset, indexDataSize);
      if (indexData.empty()) return;

      // Find Min/Max Index to minimize vertex buffer read
      uint32_t minIndex = 0xFFFFFFFF;
      uint32_t maxIndex = 0;
      
      for (UINT i = 0; i < IndexCount; i++) {
        uint32_t idx = 0;
        if (indexSize == 4) {
          idx = *reinterpret_cast<uint32_t*>(&indexData[i * 4]);
        } else {
          idx = *reinterpret_cast<uint16_t*>(&indexData[i * 2]);
        }
        
        if (idx < minIndex) minIndex = idx;
        if (idx > maxIndex) maxIndex = idx;
      }
      
      if (minIndex > maxIndex) return;

      // Read Vertex Buffer
      int32_t actualMinIndex = (int32_t)minIndex + BaseVertexLocation;
      int32_t actualMaxIndex = (int32_t)maxIndex + BaseVertexLocation;
      
      if (actualMinIndex < 0) actualMinIndex = 0;
      
      UINT vertexStride = binding.stride;
      UINT vertexOffset = binding.offset + actualMinIndex * vertexStride;
      UINT vertexCount = actualMaxIndex - actualMinIndex + 1;
      UINT vertexDataSize = vertexCount * vertexStride;
      
      std::vector<uint8_t> vertexData = ReadBuffer(pContext, binding.buffer.ptr(), vertexOffset, vertexDataSize);
      if (vertexData.empty()) return;

      // Extract Positions (Example: Log first vertex)
      if (vertexCount > 0) {
        size_t offset = attrib.offset;
        if (offset + 12 <= vertexStride) {
           float* pos = reinterpret_cast<float*>(&vertexData[offset]);
           Logger::info(str::format("D3D11Rtx: DrawIndexed First Vertex: ", pos[0], ", ", pos[1], ", ", pos[2]));
        }
      }
  }

  void D3D11Rtx::DrawInstanced(
    D3D11DeviceContext* pContext,
    UINT VertexCountPerInstance,
    UINT InstanceCount,
    UINT StartVertexLocation,
    UINT StartInstanceLocation) {
      // Placeholder: Implement RTX logic here
  }

  void D3D11Rtx::DrawIndexedInstanced(
    D3D11DeviceContext* pContext,
    UINT IndexCountPerInstance,
    UINT InstanceCount,
    UINT StartIndexLocation,
    INT  BaseVertexLocation,
    UINT StartInstanceLocation) {
      // Placeholder: Implement RTX logic here
  }
}
