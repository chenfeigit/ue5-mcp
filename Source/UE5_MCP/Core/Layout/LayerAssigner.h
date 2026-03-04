#pragma once

#include "BlueprintGraphLayoutTypes.h"

/** Phase1: Assign layer per node from exec edges. Back edges do not increase layer. */
class FLayerAssigner
{
public:
	/** Assign Layer in NodeInfos. Uses existing ChainId from ChainPartition. */
	static void AssignLayers(
		const FExecGraphResult& ExecResult,
		const TArray<FChainInfo>& Chains,
		TArray<FNodeLayoutInfo>& NodeInfos,
		const TMap<UEdGraphNode*, int32>& NodeToIndex);
};
