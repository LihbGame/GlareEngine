#include "InstanceModel.h"

InstanceModel::InstanceModel(unique_ptr<InstanceRenderData> InstanceData)
{
	mInstanceData = std::move(InstanceData);
}

void InstanceModel::Draw(GraphicsContext& Context)
{
}

void InstanceModel::BuildPSO(const RootSignature& rootSignature)
{
}
