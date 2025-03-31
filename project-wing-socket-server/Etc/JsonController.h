#pragma once

#include "GameInfo.h"
#include "Etc/JsonContainer.h"

class CJsonController
{
public:
	template<typename T>
	nlohmann::json ConvertToJson(const T& data);

	template<typename T>
	bool ParseJson(const nlohmann::json& json, T& data);

	template<typename T>
	bool ParseJson(const nlohmann::json& json, std::map<int,T>& datas);

	template<typename T>
	bool ParseJson(const nlohmann::json& json, std::map<std::string, T>& datas);

	FLineNode ParseJsonFLineNode(const nlohmann::json& json);

	FSpriteSheetInfo ParseJsonFSpriteSheetInfo(const nlohmann::json& json);

private:
	DECLARE_SINGLE(CJsonController);
};
