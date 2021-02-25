#pragma once
#include "L3DUtil.h"


namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		class RootSignature;


		class IndirectParameter
		{
			friend class CommandSignature;
		public:

			IndirectParameter()
			{
				m_IndirectParam.Type = (D3D12_INDIRECT_ARGUMENT_TYPE)0xFFFFFFFF;
			}

			void Draw(void)
			{
				m_IndirectParam.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
			}

			void DrawIndexed(void)
			{
				m_IndirectParam.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
			}

			void Dispatch(void)
			{
				m_IndirectParam.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
			}

			void VertexBufferView(UINT Slot)
			{
				m_IndirectParam.Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
				m_IndirectParam.VertexBuffer.Slot = Slot;
			}

			void IndexBufferView(void)
			{
				m_IndirectParam.Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
			}

			void Constant(UINT RootParameterIndex, UINT DestOffsetIn32BitValues, UINT Num32BitValuesToSet)
			{
				m_IndirectParam.Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
				m_IndirectParam.Constant.RootParameterIndex = RootParameterIndex;
				m_IndirectParam.Constant.DestOffsetIn32BitValues = DestOffsetIn32BitValues;
				m_IndirectParam.Constant.Num32BitValuesToSet = Num32BitValuesToSet;
			}

			void ConstantBufferView(UINT RootParameterIndex)
			{
				m_IndirectParam.Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
				m_IndirectParam.ConstantBufferView.RootParameterIndex = RootParameterIndex;
			}

			void ShaderResourceView(UINT RootParameterIndex)
			{
				m_IndirectParam.Type = D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW;
				m_IndirectParam.ShaderResourceView.RootParameterIndex = RootParameterIndex;
			}

			void UnorderedAccessView(UINT RootParameterIndex)
			{
				m_IndirectParam.Type = D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW;
				m_IndirectParam.UnorderedAccessView.RootParameterIndex = RootParameterIndex;
			}

			const D3D12_INDIRECT_ARGUMENT_DESC& GetDesc(void) const { return m_IndirectParam; }

		protected:

			D3D12_INDIRECT_ARGUMENT_DESC m_IndirectParam;
		};


		class CommandSignature
		{
		};
	}
}

