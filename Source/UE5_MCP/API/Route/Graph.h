#pragma once
#include "HttpServerRequest.h"

bool AddNodeToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete);

bool ConnectPinsHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete);

bool BreakPinConnectionHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete);

bool SetPinDefaultValueHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete);

bool GetSupportedNodesHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete);

bool RefreshSupportedNodesCacheHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete);

bool LayoutGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete);

