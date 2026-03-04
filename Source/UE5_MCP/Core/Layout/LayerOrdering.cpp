#include "LayerOrdering.h"
#include "EdGraph/EdGraphNode.h"
#include "Math/NumericLimits.h"

void FLayerOrdering::Order(
	const TArray<FChainInfo>& Chains,
	const FGraphLayoutParams& Params,
	TArray<FNodeLayoutInfo>& NodeInfos,
	const FExecGraphResult& ExecResult)
{
	// Build reverse map: target node -> list of predecessor nodes (for min LocalY tie-breaker)
	TMap<UEdGraphNode*, TArray<UEdGraphNode*>> Predecessors;
	for (const auto& Kv : ExecResult.Successors)
	{
		UEdGraphNode* Source = Kv.Key;
		for (const FExecSuccessor& S : Kv.Value)
		{
			if (S.Target)
			{
				Predecessors.FindOrAdd(S.Target).Add(Source);
			}
		}
	}

	for (const FChainInfo& Chain : Chains)
	{
		if (Chain.Nodes.Num() == 0) continue;

		// Node -> layout info for this chain (to look up predecessor LocalY)
		TMap<UEdGraphNode*, FNodeLayoutInfo*> NodeToInfo;
		for (FNodeLayoutInfo* P : Chain.Nodes)
		{
			if (P && P->Node) NodeToInfo.Add(P->Node, P);
		}

		// Group by layer
		TMap<int32, TArray<FNodeLayoutInfo*>> LayerToNodes;
		for (FNodeLayoutInfo* P : Chain.Nodes)
		{
			if (!P) continue;
			LayerToNodes.FindOrAdd(P->Layer).Add(P);
		}

		// Sort each layer by (min predecessor LocalY, SubIndex, NodeGuid) for multi-level branches; assign LocalY
		TArray<int32> Layers;
		LayerToNodes.GetKeys(Layers);
		Layers.Sort();
		for (int32 Layer : Layers)
		{
			TArray<FNodeLayoutInfo*>& Nodes = LayerToNodes[Layer];
			Nodes.Sort([&Predecessors, &NodeToInfo](const FNodeLayoutInfo& A, const FNodeLayoutInfo& B)
			{
				// Primary: group by branch (predecessor Y) so same-branch nodes stay together, avoiding cross-linked wires
				float MinPredLocalY_A = TNumericLimits<float>::Max();
				if (const TArray<UEdGraphNode*>* Preds = Predecessors.Find(A.Node))
				{
					for (UEdGraphNode* Pred : *Preds)
					{
						if (FNodeLayoutInfo* const* Info = NodeToInfo.Find(Pred))
							MinPredLocalY_A = FMath::Min(MinPredLocalY_A, (*Info)->LocalY);
					}
				}
				float MinPredLocalY_B = TNumericLimits<float>::Max();
				if (const TArray<UEdGraphNode*>* Preds = Predecessors.Find(B.Node))
				{
					for (UEdGraphNode* Pred : *Preds)
					{
						if (FNodeLayoutInfo* const* Info = NodeToInfo.Find(Pred))
							MinPredLocalY_B = FMath::Min(MinPredLocalY_B, (*Info)->LocalY);
					}
				}
				if (MinPredLocalY_A != MinPredLocalY_B) return MinPredLocalY_A < MinPredLocalY_B;
				// Secondary: within same branch, order by pin (Then 0 before Then 1, etc.)
				if (A.SubIndex != B.SubIndex) return A.SubIndex < B.SubIndex;
				if (A.Node && B.Node) return A.Node->NodeGuid < B.Node->NodeGuid;
				return false;
			});
			float Y = 0.f;
			for (FNodeLayoutInfo* P : Nodes)
			{
				if (P) { P->LocalY = Y; Y += Params.VSpacing; }
			}
		}
	}
}
