#include "BlueprintGraphLayout.h"
#include "ChainPartition.h"
#include "Engine/Blueprint.h"
#include "ExecGraphBuilder.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "LayerAssigner.h"
#include "LayerOrdering.h"
#include "LayoutPlacer.h"

void FBlueprintGraphLayout::LayoutGraph(UBlueprint* BP, UEdGraph* Graph)
{
	if (!BP || !Graph) return;

	FExecGraphResult ExecResult;
	FExecGraphBuilder::Build(Graph, ExecResult);

	TArray<FNodeLayoutInfo> NodeInfos;
	TMap<UEdGraphNode*, int32> NodeToIndex;
	int32 Idx = 0;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;
		FNodeLayoutInfo Info;
		Info.Node = Node;
		Info.bPure = ExecResult.PureNodes.Contains(Node);
		NodeInfos.Add(Info);
		NodeToIndex.Add(Node, Idx++);
	}

	FGraphLayoutParams Params;
	TArray<FChainInfo> Chains;
	FChainPartition::Partition(Graph, ExecResult, Params, NodeInfos, NodeToIndex, Chains);
	FLayerAssigner::AssignLayers(ExecResult, Chains, NodeInfos, NodeToIndex);
	FLayerOrdering::Order(Chains, Params, NodeInfos, ExecResult);
	FLayoutPlacer::Place(Chains, ExecResult, Params, NodeInfos);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
}
