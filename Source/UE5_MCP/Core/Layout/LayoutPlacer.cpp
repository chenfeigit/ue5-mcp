#include "LayoutPlacer.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

namespace
{
	/** Collect pure predecessors of P by following input data links backwards. Order: sources first (leftmost), direct preds last (rightmost before P). */
	void CollectPurePredecessors(
		UEdGraphNode* P,
		const FExecGraphResult& ExecResult,
		const FName ExecCategory,
		TArray<UEdGraphNode*>& OutOrderedPredecessors)
	{
		OutOrderedPredecessors.Empty();
		if (!P || !ExecResult.PureNodes.Contains(P)) return;

		TSet<UEdGraphNode*> Visited;
		TArray<TPair<UEdGraphNode*, int32>> PredsWithDist;
		TArray<TPair<UEdGraphNode*, int32>> Queue;
		Queue.Add({ P, 0 });
		Visited.Add(P);

		while (Queue.Num() > 0)
		{
			UEdGraphNode* N = Queue[0].Key;
			const int32 Dist = Queue[0].Value;
			Queue.RemoveAt(0);

			for (UEdGraphPin* Pin : N->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input || Pin->PinType.PinCategory == ExecCategory) continue;
				for (UEdGraphPin* FromPin : Pin->LinkedTo)
				{
					if (!FromPin) continue;
					UEdGraphNode* Q = FromPin->GetOwningNode();
					if (!Q || !ExecResult.PureNodes.Contains(Q) || Visited.Contains(Q)) continue;
					Visited.Add(Q);
					const int32 NextDist = Dist + 1;
					PredsWithDist.Add({ Q, NextDist });
					Queue.Add({ Q, NextDist });
				}
			}
		}

		PredsWithDist.Sort([](const TPair<UEdGraphNode*, int32>& A, const TPair<UEdGraphNode*, int32>& B) { return A.Value > B.Value; });
		for (const auto& Pair : PredsWithDist)
		{
			OutOrderedPredecessors.Add(Pair.Key);
		}
	}
}

void FLayoutPlacer::Place(
	const TArray<FChainInfo>& Chains,
	const FExecGraphResult& ExecResult,
	const FGraphLayoutParams& Params,
	const TArray<FNodeLayoutInfo>& NodeInfos)
{
	const FName ExecCategory = UEdGraphSchema_K2::PC_Exec;
	float MaxY = 0.f;
	for (const FChainInfo& C : Chains)
	{
		MaxY = FMath::Max(MaxY, C.BaseY + C.Height);
	}

	// Place exec nodes first
	for (const FNodeLayoutInfo& Info : NodeInfos)
	{
		if (!Info.Node || Info.bPure) continue;
		if (Info.ChainId < 0 || !Chains.IsValidIndex(Info.ChainId)) continue;
		const FChainInfo& Chain = Chains[Info.ChainId];
		Info.Node->NodePosX = Info.Layer * Params.HSpacing;
		Info.Node->NodePosY = Chain.BaseY + Info.LocalY;
	}

	// Build: exec node -> list of param (pure) nodes that have a data link into it.
	// Traverse from exec nodes' input (data) pins so we reliably find all param connections.
	TMap<UEdGraphNode*, TArray<UEdGraphNode*>> ExecToParamNodes;
	for (const FNodeLayoutInfo& Info : NodeInfos)
	{
		if (!Info.Node || Info.bPure) continue;
		UEdGraphNode* ExecNode = Info.Node;
		for (UEdGraphPin* Pin : ExecNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || Pin->PinType.PinCategory == ExecCategory) continue;
			for (UEdGraphPin* FromPin : Pin->LinkedTo)
			{
				if (!FromPin) continue;
				UEdGraphNode* SourceNode = FromPin->GetOwningNode();
				if (!SourceNode || !ExecResult.PureNodes.Contains(SourceNode)) continue;
				TArray<UEdGraphNode*>& List = ExecToParamNodes.FindOrAdd(ExecNode);
				List.AddUnique(SourceNode);
			}
		}
	}

	// Place param nodes and their pure chains: to the left of exec; same exec -> vertical stack per chain
	TSet<UEdGraphNode*> PlacedPure;
	for (const auto& Pair : ExecToParamNodes)
	{
		UEdGraphNode* ExecNode = Pair.Key;
		TArray<UEdGraphNode*> ParamList = Pair.Value;
		ParamList.Sort([](const UEdGraphNode& A, const UEdGraphNode& B) { return A.NodeGuid < B.NodeGuid; });
		const float ExecX = ExecNode->NodePosX;
		const float ExecY = ExecNode->NodePosY;
		int32 Slot = 0;
		for (UEdGraphNode* P : ParamList)
		{
			if (!P || PlacedPure.Contains(P)) continue;
			TArray<UEdGraphNode*> Predecessors;
			CollectPurePredecessors(P, ExecResult, ExecCategory, Predecessors);
			TArray<UEdGraphNode*> ToPlace;
			for (UEdGraphNode* Pred : Predecessors)
			{
				if (Pred && !PlacedPure.Contains(Pred)) ToPlace.Add(Pred);
			}
			ToPlace.Add(P);
			const float BaseX = ExecX - Params.ParamNodeOffsetX;
			const float BaseY = ExecY + Params.ParamBaseOffsetY + Slot * Params.ParamNodeSpacing;
			const int32 N = ToPlace.Num();
			for (int32 i = 0; i < N; i++)
			{
				UEdGraphNode* Node = ToPlace[i];
				Node->NodePosX = BaseX - (N - 1 - i) * Params.HSpacing;
				Node->NodePosY = BaseY;
				PlacedPure.Add(Node);
			}
			++Slot;
		}
	}

	// Pure nodes with no exec link: band below exec chains
	for (const FNodeLayoutInfo& Info : NodeInfos)
	{
		if (!Info.Node || !Info.bPure || PlacedPure.Contains(Info.Node)) continue;
		Info.Node->NodePosX = 0;
		Info.Node->NodePosY = MaxY;
		MaxY += Params.VSpacing;
	}
}
