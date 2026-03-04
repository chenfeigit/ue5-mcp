#include "Graph.h"

#include "UE5_MCP/API/Utils.h"
#include "UE5_MCP/API/DTO/Graph/AddEventToGraphReq.h"
#include "UE5_MCP/API/DTO/Graph/GraphOperationReq.h"
#include "UE5_MCP/Core/Layout/BlueprintGraphLayout.h"
#include "UE5_MCP/API/DTO/Graph/AddFunctionCallToGraphReq.h"
#include "UE5_MCP/API/DTO/Graph/AddVariableToGraphReq.h"
#include "UE5_MCP/API/DTO/Graph/GenericAddNodeToGraphReq.h"
#include "UE5_MCP/API/DTO/Graph/PinOperationReq.h"
#include "UE5_MCP/API/DTO/Graph/SetPinDefaultValueReq.h"
#include "UE5_MCP/Core/BPUtils.h"
#include "UE5_MCP/Core/GraphUtils.h"

/** Resolve graph by name: event graph (UbergraphPages) first, then function graph (FunctionGraphs). */
static UEdGraph* GetGraphByName(UBlueprint* BP, const FString& GraphName)
{
	if (!BP) return nullptr;
	UEdGraph* G = BPUtils::GetEventGraph(BP, GraphName);
	if (!G) G = BPUtils::GetFunctionGraph(BP, GraphName);
	return G;
}

bool AddEventToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try
	{
		FAddEventToGraphReq body = Utils::BufferToJson<FAddEventToGraphReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		if (body.bIsCustomEvent)
			GraphUtils::AddCustomEventToGraph(BP, Graph, body.EventName, body.EventSignature);
		else
			GraphUtils::AddEventToGraph(BP, Graph, body.EventName);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool AddVariableToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try
	{
		FAddVariableToGraphReq body = Utils::BufferToJson<FAddVariableToGraphReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		if (body.bIsSetter)
			GraphUtils::AddSetVariableNodeToGraph(BP, Graph, body.VarName);
		else
			GraphUtils::AddGetVariableNodeToGraph(BP, Graph, body.VarName);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool AddFunctionCallToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try
	{
		FAddFunctionCallToGraphReq body = Utils::BufferToJson<FAddFunctionCallToGraphReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph) throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		if (body.ClassToCall.IsEmpty())
			GraphUtils::AddFunctionCallToGraph(BP, Graph, body.FunctionName);
		else
			GraphUtils::AddFunctionCallToGraph(BP, Graph, body.ClassToCall, body.FunctionName);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool ConnectPinsHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		FPinOperationReq body = Utils::BufferToJson<FPinOperationReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph) throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::ConnectPins(BP, Graph, body.OutputNodeId, body.OutputPinName, body.InputNodeId, body.InputPinName);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}

	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool BreakPinConnectionHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		FPinOperationReq body = Utils::BufferToJson<FPinOperationReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::BreakPinConnection(BP, Graph, body.OutputNodeId, body.OutputPinName, body.InputNodeId, body.InputPinName);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool SetPinDefaultValueHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		FSetPinDefaultValueReq body = Utils::BufferToJson<FSetPinDefaultValueReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::SetPinDefaultValue(
			BP,
			Graph,
			body.NodeId,
			body.PinName,
			body.DefaultValue);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool GetSupportedNodesHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		TArray<FString> Nodes = GraphUtils::GetSupportedNode();
		FString Json = Utils::ToJsonString(Nodes);
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(Json, TEXT("application/json"));
		Resp->Code = EHttpServerResponseCodes::Ok;
		OnComplete(MoveTemp(Resp));
		return true;
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	
}

bool AddGenericNodeToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		FGenericAddNodeToGraphReq body = Utils::BufferToJson<FGenericAddNodeToGraphReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph) throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::AddNodeByNameToGraph(BP, Graph, body.NodeTypeName);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool AddMakeStructNodeToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		FGenericAddNodeToGraphReq body = Utils::BufferToJson<FGenericAddNodeToGraphReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::AddMakeStructNodeToGraph(BP, Graph, body.ExtraInfo);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool AddBreakStructNodeToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		FGenericAddNodeToGraphReq body = Utils::BufferToJson<FGenericAddNodeToGraphReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::AddBreakStructNodeToGraph(BP, Graph, body.ExtraInfo);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

/*
bool AddSwitchEnumNodeToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		FGenericAddNodeToGraphReq body = Utils::BufferToJson<FGenericAddNodeToGraphReq>(Req.Body);
		GraphUtils::AddSwitchEnumNodeToGraph(
			BPUtils::LoadBlueprint(body.BpPath),
			BPUtils::GetEventGraph(BPUtils::LoadBlueprint(body.BpPath),
				body.GraphName),
			body.ExtraInfo);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}
*/

bool AddDynamicCastNodeToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		FGenericAddNodeToGraphReq body = Utils::BufferToJson<FGenericAddNodeToGraphReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::AddDynamicCastNodeToGraph(BP, Graph, body.ExtraInfo);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool AddClassCastNodeToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		FGenericAddNodeToGraphReq body = Utils::BufferToJson<FGenericAddNodeToGraphReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::AddClassCastNodeToGraph(BP, Graph, body.ExtraInfo);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool AddEnumCastNodeToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		FGenericAddNodeToGraphReq body = Utils::BufferToJson<FGenericAddNodeToGraphReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::AddByteToEnumNodeCastToGraph(BP, Graph, body.ExtraInfo);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool AddMathNodeToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try
	{
		FAddFunctionCallToGraphReq body = Utils::BufferToJson<FAddFunctionCallToGraphReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::AddMathFunctionCallToGraph(BP, Graph, body.FunctionName);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool LayoutGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try
	{
		FGraphOperationReq body = Utils::BufferToJson<FGraphOperationReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		FBlueprintGraphLayout::LayoutGraph(BP, Graph);
	}
	catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}

bool AddCommentNodeToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try {
		FGenericAddNodeToGraphReq body = Utils::BufferToJson<FGenericAddNodeToGraphReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph)
			throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::AddCommentNodeToGraph(BP, Graph, body.ExtraInfo);
	} catch (std::runtime_error& e)
	{
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(
			FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())), TEXT("text/plain"));
		Resp->Code = EHttpServerResponseCodes::ServerError;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
	return true;
}
