#include "ChainPartition.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"

void FChainPartition::Partition(
	UEdGraph* Graph,
	const FExecGraphResult& ExecResult,
	const FGraphLayoutParams& Params,
	TArray<FNodeLayoutInfo>& NodeInfos,
	const TMap<UEdGraphNode*, int32>& NodeToIndex,
	TArray<FChainInfo>& OutChains)
{
	OutChains.Empty();

	TSet<UEdGraphNode*> Assigned;
	TArray<UEdGraphNode*> SortedRoots = ExecResult.Roots;
	SortedRoots.Sort([](const UEdGraphNode& A, const UEdGraphNode& B) { return A.NodeGuid < B.NodeGuid; });

	// BFS from each root; first root to reach a node wins that node for its chain
	for (int32 ChainId = 0; ChainId < SortedRoots.Num(); ChainId++)
	{
		UEdGraphNode* Root = SortedRoots[ChainId];
		if (!Root) continue;

		TArray<UEdGraphNode*> Queue;
		Queue.Add(Root);
		FChainInfo Chain;
		Chain.BaseY = 0.f;
		Chain.Height = 0.f;

		while (Queue.Num() > 0)
		{
			UEdGraphNode* N = Queue.Pop();
			if (Assigned.Contains(N)) continue;
			Assigned.Add(N);

			const int32* Idx = NodeToIndex.Find(N);
			if (Idx && NodeInfos.IsValidIndex(*Idx))
			{
				FNodeLayoutInfo& Info = NodeInfos[*Idx];
				Info.ChainId = ChainId;
				Info.bOrphan = false;
				Chain.Nodes.Add(&Info);
			}

			if (const TArray<FExecSuccessor>* Succ = ExecResult.Successors.Find(N))
			{
				for (const FExecSuccessor& S : *Succ)
				{
					if (S.Target && !Assigned.Contains(S.Target)) Queue.Add(S.Target);
				}
			}
		}

		// Estimate height by node count
		Chain.Height = Chain.Nodes.Num() * Params.VSpacing;
		OutChains.Add(MoveTemp(Chain));
	}

	// Single orphan chain: all nodes not reached from any root (excluding pure)
	FChainInfo OrphanChain;
	OrphanChain.BaseY = 0.f;
	OrphanChain.Height = Params.OrphanBandHeight;
	const int32 OrphanChainId = OutChains.Num();
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node || ExecResult.PureNodes.Contains(Node)) continue;
		if (Assigned.Contains(Node)) continue;

		const int32* Idx = NodeToIndex.Find(Node);
		if (Idx && NodeInfos.IsValidIndex(*Idx))
		{
			FNodeLayoutInfo& Info = NodeInfos[*Idx];
			Info.ChainId = OrphanChainId;
			Info.bOrphan = true;
			OrphanChain.Nodes.Add(&Info);
		}
	}
	if (OrphanChain.Nodes.Num() > 0)
	{
		OrphanChain.Height = OrphanChain.Nodes.Num() * Params.VSpacing;
		OutChains.Add(MoveTemp(OrphanChain));
	}

	// Assign BaseY: stack chains vertically
	float Y = 0.f;
	for (FChainInfo& C : OutChains)
	{
		C.BaseY = Y;
		Y += C.Height + Params.VSpacing;
	}
}
