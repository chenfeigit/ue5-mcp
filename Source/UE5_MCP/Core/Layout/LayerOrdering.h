#pragma once

#include "BlueprintGraphLayoutTypes.h"

/** Phase2: Order nodes within each layer by min predecessor LocalY, SubIndex, and NodeGuid; assign LocalY. Supports multi-level branches and avoids cross-linked wires. */
class FLayerOrdering
{
public:
	/** Set LocalY in NodeInfos per chain. Sort key (min predecessor LocalY, SubIndex, NodeGuid) keeps same-branch nodes vertically grouped, avoiding wire crossings across branch levels. */
	static void Order(
		const TArray<FChainInfo>& Chains,
		const FGraphLayoutParams& Params,
		TArray<FNodeLayoutInfo>& NodeInfos,
		const FExecGraphResult& ExecResult);
};
