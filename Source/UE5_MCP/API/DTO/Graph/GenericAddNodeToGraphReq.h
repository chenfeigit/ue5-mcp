#pragma once
#include "CoreMinimal.h"
#include "GraphOperationReq.h"
#include "GenericAddNodeToGraphReq.generated.h"

/** Unified request for adding any node type to a graph. NodeTypeName + optional fields determine the node and binding. */
USTRUCT()
struct FGenericAddNodeToGraphReq : public FGraphOperationReq
{
	GENERATED_BODY()

	UPROPERTY()
	FString NodeTypeName;

	UPROPERTY()
	FString ExtraInfo; // e.g. StructTypeName for Make/Break Struct, class/enum name for cast nodes, comment text, etc.

	// CallFunction binding (when node_type_name is K2Node_CallFunction or ClassName::FunctionName)
	UPROPERTY()
	FString ClassToCall;

	UPROPERTY()
	FString FunctionName;

	// VariableGet / VariableSet
	UPROPERTY()
	FString VarName;

	UPROPERTY()
	bool bIsSetter = false;

	// Event / CustomEvent
	UPROPERTY()
	FString EventName;

	UPROPERTY()
	bool bIsCustomEvent = false;

	UPROPERTY()
	FString EventSignature;

	// Optional: generic member name, mapped by node type to VarName/EventName/FunctionName+ClassToCall
	UPROPERTY()
	FString MemberName;
};
