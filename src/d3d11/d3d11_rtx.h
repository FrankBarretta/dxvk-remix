#pragma once

#include "d3d11_device.h"
#include "../dxvk/dxvk_buffer.h"

namespace dxvk {
  class D3D11Device;
  class D3D11DeviceContext;

  struct D3D11Rtx {
    D3D11Rtx(D3D11Device* d3d11Device);
    ~D3D11Rtx();

    void Draw(
      D3D11DeviceContext* pContext,
      UINT VertexCount,
      UINT StartVertexLocation);

    void DrawIndexed(
      D3D11DeviceContext* pContext,
      UINT IndexCount,
      UINT StartIndexLocation,
      INT  BaseVertexLocation);

    void DrawInstanced(
      D3D11DeviceContext* pContext,
      UINT VertexCountPerInstance,
      UINT InstanceCount,
      UINT StartVertexLocation,
      UINT StartInstanceLocation);

    void DrawIndexedInstanced(
      D3D11DeviceContext* pContext,
      UINT IndexCountPerInstance,
      UINT InstanceCount,
      UINT StartIndexLocation,
      INT  BaseVertexLocation,
      UINT StartInstanceLocation);

  private:
    D3D11Device* m_parent;
  };
}
