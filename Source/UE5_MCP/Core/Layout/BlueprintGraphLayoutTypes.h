#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"

/** Layout parameters for graph placement. */
struct FGraphLayoutParams
{
	int32 HSpacing = 400;
	int32 VSpacing = 200;
	int32 OrphanBandHeight = 600;
	/** Horizontal offset: param nodes are placed this many units to the left of their exec node. */
	int32 ParamNodeOffsetX = 350;
	/** Vertical spacing between param nodes that feed the same exec node (avoids overlap). */
	int32 ParamNodeSpacing = 120;
	/** Vertical offset: param nodes sit this many units below the exec node baseline. */
	float ParamBaseOffsetY = 0.f;
};

/** Per-node layout info produced by layout phases. */
struct FNodeLayoutInfo
{
	UEdGraphNode* Node = nullptr;
	int32 ChainId = -1;
	int32 Layer = 0;
	int32 SubIndex = 0;
	float LocalY = 0.f;
	bool bPure = false;
	bool bOrphan = false;
};

/** One chain (event/root subgraph) with Y band. */
struct FChainInfo
{
	TArray<FNodeLayoutInfo*> Nodes;
	float BaseY = 0.f;
	float Height = 0.f;
};

/** Exec edge: target node and output pin subIndex (for Branch/Sequence/Switch order). */
struct FExecSuccessor
{
	UEdGraphNode* Target = nullptr;
	int32 SubIndex = 0;
};

/** Result of building exec graph: roots and per-node exec successors. */
struct FExecGraphResult
{
	TArray<UEdGraphNode*> Roots;
	TMap<UEdGraphNode*, TArray<FExecSuccessor>> Successors;
	TSet<UEdGraphNode*> PureNodes;
};
