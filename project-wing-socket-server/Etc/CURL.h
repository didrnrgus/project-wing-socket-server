#pragma once

#include "GameInfo.h"
 
#define HEADER_AUTHORIZATION "Authorization: "
#define HEADER_NOTION_VERSION "Notion-Version: "
#define HEADER_CONTENT_TYPE "Content-Type: application/json"

#define CACERT_PATH "./pem/cacert.pem"
#define WEBSERVER_PATH "https://didrnrgus.github.io/project-wing-webserver/"
#define CONFIG_PATH "config.json"

#define METHOD_POST "POST"
#define METHOD_PATCH "PATCH"
#define METHOD_DELETE "DELETE"
#define METHOD_GET "GET"

class CCURL
{
public:
    std::string SendRequest(const std::string& InURL, const std::string& InMethod, const std::string& InJsonData = "");

    DECLARE_SINGLE(CCURL);
};
