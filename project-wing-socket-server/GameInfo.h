#pragma once

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <Windows.h>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#pragma comment(lib, "libcurl-x64.lib")

#define	SAFE_DELETE(p)	if(p)	{ delete p; p = nullptr; }
#define	SAFE_DELETE_ARRAY(p)	if(p)	{ delete[] p; p = nullptr; }
#define	SAFE_RELEASE(p)	if(p)	{ p->Release(); p = nullptr; }

#define	DECLARE_SINGLE(Type)	\
private:\
	Type();\
	~Type();\
private:\
	static Type*	mInst;\
public:\
	static Type* GetInst()\
	{\
		if(!mInst)\
			mInst = new Type;\
		return mInst;\
	}\
	static void DestroyInst()\
	{\
		SAFE_DELETE(mInst);\
	}

#define	DEFINITION_SINGLE(Type)	Type* Type::mInst = nullptr;

template<typename T>
const T& clamp(const T & v, const T & lo, const T & hi) {
	return (v < lo) ? lo : (hi < v) ? hi : v;
}