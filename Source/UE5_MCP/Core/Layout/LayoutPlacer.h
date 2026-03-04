#pragma once

#include "BlueprintGraphLayoutTypes.h"

/** Phase3: Write NodePosX / NodePosY from chains, layers, LocalY. Param nodes (pure nodes linked to exec) are placed to the left and slightly below their exec node; same exec params stack vertically without overlap. Other pure nodes go in a band below chains. */
class FLayoutPlacer
{
public:
	/** Apply positions: exec nodes by chain/layer; param nodes by reference exec (left + below, no overlap); remaining pure nodes in bottom band. */
	static void Place(
		const TArray<FChainInfo>& Chains,
		const FExecGraphResult& ExecResult,
		const FGraphLayoutParams& Params,
		const TArray<FNodeLayoutInfo>& NodeInfos);
};
