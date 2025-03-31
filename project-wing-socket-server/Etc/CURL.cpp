#include "GameInfo.h"
#include "Etc/CURL.h"
//#include "Etc/NotionDBController.h"

DEFINITION_SINGLE(CCURL);

CCURL::CCURL()
{
	curl_global_init(CURL_GLOBAL_ALL);
}

CCURL::~CCURL()
{

}

// 응답 데이터 저장 콜백 함수
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output)
{
	size_t totalSize = size * nmemb;
	output->append((char*)contents, totalSize);
	return totalSize;
}

std::string CCURL::SendRequest(const std::string& InURL
	, const std::string& InMethod
	, const std::string& InJsonData)
{
	CURL* curl = curl_easy_init();
	if (!curl) return "Failed to initialize cURL";

	// SSL 인증서 파일 설정 (SSL 검증을 위한 인증서 경로)
	curl_easy_setopt(curl, CURLOPT_CAINFO, CACERT_PATH);

	std::string response;
	//std::string auth = HEADER_AUTHORIZATION + CNotionDBController::GetInst()->GetNotionAPIKey();
	//std::string version = HEADER_NOTION_VERSION + CNotionDBController::GetInst()->GetLastUpdateDate();

	curl_easy_setopt(curl, CURLOPT_URL, InURL.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

	struct curl_slist* headers = NULL;

	if (InMethod != METHOD_GET)
	{
		// 헤더 추가
		//headers = curl_slist_append(headers, auth.c_str());
		//headers = curl_slist_append(headers, version.c_str());
		//headers = curl_slist_append(headers, HEADER_CONTENT_TYPE);

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		// HTTP Method 설정
		if (InMethod == METHOD_POST)
		{
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, InJsonData.c_str());
		}
		else if (InMethod == METHOD_PATCH)
		{
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, METHOD_PATCH);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, InJsonData.c_str());
		}
		else if (InMethod == METHOD_DELETE)
		{
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, METHOD_DELETE);
		}
	}

	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		std::cerr << "cURL request failed: " << curl_easy_strerror(res) << "\n";
	}

	long response_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
	std::cout << "HTTP Response Code: " << response_code << std::endl;
	std::cout << "Response Body: " << response << std::endl;

	// 리소스 해제
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	return response;
}
