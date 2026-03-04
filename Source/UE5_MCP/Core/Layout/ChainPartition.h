#pragma once

#include "BlueprintGraphLayoutTypes.h"

/** Phase0: Partition nodes into chains by root BFS, assign Y bands. Orphans go to last chain. */
class FChainPartition
{
public:
	/** Partition graph into chains. Fills NodeInfos[].ChainId, .bOrphan; builds OutChains (BaseY, Height, Nodes). */
	static void Partition(
		UEdGraph* Graph,
		const FExecGraphResult& ExecResult,
		const FGraphLayoutParams& Params,
		TArray<FNodeLayoutInfo>& NodeInfos,
		const TMap<UEdGraphNode*, int32>& NodeToIndex,
		TArray<FChainInfo>& OutChains);
};
