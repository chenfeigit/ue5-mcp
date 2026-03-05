#pragma once
#include <stdexcept>

// Utility functions for manipulating Blueprint graphs
class GraphUtils
{
	static UEdGraphNode* GetNodeById(UEdGraph* Graph, const FGuid& NodeId);

	static UClass* FindK2NodeClassByName(const FString& NodeTypeName);

	/** Resolves a macro graph by name from the engine StandardMacros blueprint. Returns nullptr if not found or load fails. */
	static class UEdGraph* FindMacroGraphByName(const FString& MacroGraphName);

	/** Places NewNode to the right of the rightmost existing node to avoid overlap. Used by all Add*ToGraph. */
	static void SetNewNodePosition(UEdGraph* Graph, UEdGraphNode* NewNode);

	static const int32 DefaultNewNodeSpacing;
public:
	
#pragma region AddNodes
	// Adds a function / custom event call node to the specified Blueprint's graph, calling the function from the Blueprint's own class
	// Careful: it doesn't check duplicated custom event names, may cause compile errors. Returns the new node.
	static UEdGraphNode* AddFunctionCallToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& FunctionName);

	// Adds a function call node to the specified Blueprint's graph, specifying the class to call the function from. Returns the new node.
	static UEdGraphNode* AddFunctionCallToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& ClassToCall, const FString& FunctionName);

	// Adds a math function call node to the specified Blueprint's graph, calling the function from UKismetMathLibrary. Returns the new node.
	static UEdGraphNode* AddMathFunctionCallToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& FunctionName);

	// Adds an existed event to the specified Blueprint's event graph. Returns the new node.
	// Careful: it doesn't check duplicated event names, may cause compile errors
	static UEdGraphNode* AddEventToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& EventName);
	
	// Adds a custom event to the specified Blueprint's graph. Returns the new node.
	static UEdGraphNode* AddCustomEventToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& EventName, const FString& EventSignature);

	// Adds a variable get node to the specified Blueprint's graph. Returns the new node.
	static UEdGraphNode* AddGetVariableNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& VarName);

	// Adds a variable set node to the specified Blueprint's graph. Returns the new node.
	static UEdGraphNode* AddSetVariableNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& VarName);

	// Adds a Break Struct node to the specified Blueprint's graph. Returns the new node.
	static UEdGraphNode* AddBreakStructNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& StructTypeName);

	// Adds a Make Struct node to the specified Blueprint's graph. Returns the new node.
	static UEdGraphNode* AddMakeStructNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& StructTypeName);

	// Adds a Comment node to the specified Blueprint's graph. Returns the new node.
	static UEdGraphNode* AddCommentNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& CommentText);

	// Adds a Switch Enum node to the specified Blueprint's graph
	// static void AddSwitchEnumNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& PinTypeName);

	// Adds a Dynamic Cast node to the specified Blueprint's graph. Returns the new node.
	static UEdGraphNode* AddDynamicCastNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& PinTypeName);

	// Adds a Class Cast node to the specified Blueprint's graph. Returns the new node.
	static UEdGraphNode* AddClassCastNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& PinTypeName);

	// Adds a Byte to Enum Cast node to the specified Blueprint's graph. Returns the new node.
	static UEdGraphNode* AddByteToEnumNodeCastToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& PinTypeName);

	// TODO: Add support for Switch on Name, String, Int nodes
	
	// TODO: Add support for array, set, map operations nodes
	
	// TODO: Add support for UKismetXXXLibrary function nodes
	
	// TODO: Add special nodes handlers here

	// Adds a node by its type name to the specified Blueprint's graph. Returns the new node.
	// This is fallback method for adding nodes that do not have specific handling implemented
	// For nodes have implemented handling, please use the specific functions above instead
	static UEdGraphNode* AddNodeByNameToGraph(UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FString& NodeTypeName);

#pragma endregion

	// Returns a list of supported node type names that can be added via AddNodeByNameToGraph (cached after first build).
	static TArray<FString> GetSupportedNode();

	// Clears the supported nodes cache (e.g. after hot reload). Next GetSupportedNode() will rebuild.
	static void InvalidateSupportedNodesCache();

#pragma region PinOperations

	// Connects two pins from two nodes in the specified Blueprint's graph
	static void ConnectPins(UBlueprint* Blueprint,
	                                UEdGraph* Graph,
	                                const FGuid& OutputNodeId,
	                                const FString& OutputPinName,
	                                const FGuid& InputNodeId,
	                                const FString& InputPinName);

	// Breaks the connection between two pins from two nodes in the specified Blueprint's graph
	static void BreakPinConnection(UBlueprint* Blueprint,
	                                   UEdGraph* Graph,
	                                   const FGuid& OutputNodeId,
									const FString& OutputPinName,
									const FGuid& InputNodeId,
									const FString& InputPinName);

	// Sets the default value of a pin from a node in the specified Blueprint's graph
	static void SetPinDefaultValue(UBlueprint* Blueprint, UEdGraph* Graph, const FGuid& NodeId, const FString& PinName,
	                               const FString& DefaultValue);
#pragma endregion
};
