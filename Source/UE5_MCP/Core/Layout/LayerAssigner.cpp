#include "LayerAssigner.h"
#include "EdGraph/EdGraphNode.h"

void FLayerAssigner::AssignLayers(
	const FExecGraphResult& ExecResult,
	const TArray<FChainInfo>& Chains,
	TArray<FNodeLayoutInfo>& NodeInfos,
	const TMap<UEdGraphNode*, int32>& NodeToIndex)
{
	// Per chain: BFS from roots in that chain, assign layer. Back edge: min(current, source+1)
	for (const FChainInfo& Chain : Chains)
	{
		if (Chain.Nodes.Num() == 0) continue;

		for (FNodeLayoutInfo* P : Chain.Nodes) { if (P) P->Layer = -1; }
		TSet<UEdGraphNode*> InChain;
		for (FNodeLayoutInfo* P : Chain.Nodes) { if (P && P->Node) InChain.Add(P->Node); }

		TArray<UEdGraphNode*> Queue;
		for (FNodeLayoutInfo* P : Chain.Nodes)
		{
			if (!P || !P->Node) continue;
			bool bIsRoot = ExecResult.Roots.Contains(P->Node);
			if (bIsRoot)
			{
				P->Layer = 0;
				Queue.Add(P->Node);
			}
		}

		while (Queue.Num() > 0)
		{
			UEdGraphNode* N = Queue.Pop();
			const int32* MyIdx = NodeToIndex.Find(N);
			if (!MyIdx || !NodeInfos.IsValidIndex(*MyIdx)) continue;
			const int32 L = NodeInfos[*MyIdx].Layer;

			const TArray<FExecSuccessor>* Succ = ExecResult.Successors.Find(N);
			if (!Succ) continue;

			for (const FExecSuccessor& S : *Succ)
			{
				if (!S.Target || !InChain.Contains(S.Target)) continue;
				const int32* Tid = NodeToIndex.Find(S.Target);
				if (!Tid || !NodeInfos.IsValidIndex(*Tid)) continue;
				FNodeLayoutInfo& TInfo = NodeInfos[*Tid];
				int32 NewLayer = L + 1;
				if (TInfo.Layer >= 0)
					TInfo.Layer = FMath::Min(TInfo.Layer, NewLayer);
				else
				{
					TInfo.Layer = NewLayer;
					TInfo.SubIndex = S.SubIndex;
					Queue.Add(S.Target);
				}
			}
		}

		// Nodes not reached (e.g. orphan chain) keep default layer 0
		for (FNodeLayoutInfo* P : Chain.Nodes)
		{
			if (P && P->Layer < 0) P->Layer = 0;
		}
	}
}
