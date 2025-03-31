#pragma once
#include "GameInfo.h"
#include "Etc/JsonContainer.h"
#include "Etc/CURL.h"
#include "Etc/DataStorageManager.h"
#include "Etc/JsonController.h"

#define PORT 12345
#define MAX_PLAYERS 5

enum GameState { WAITING, RUNNING };

namespace ClientMessage
{
	enum Type
	{
		MSG_HEARTBEAT,
		MSG_START,
		MSG_PICK_CHARACTER,
		MSG_PICK_ITEM,
		MSG_PICK_MAP,
		MSG_READY,
		MSG_UNREADY,
		MSG_MOVE_UP,
		MSG_MOVE_DOWN,
		MSG_PLAYER_DEAD,
		MSG_TAKE_DAMAGE // ✅ HP 감소 메시지
	};
}

namespace ServerMessage
{
	enum Type
	{
		MSG_CONNECTED,
		MSG_NEW_OWNER,
		MSG_HEARTBEAT_ACK,
		MSG_START_ACK,
		MSG_JOIN,
		MSG_DISCONNECT, // 이건 누가 나간거.
		MSG_CONNECTED_REJECT,
		MSG_ROOM_FULL_INFO,
		MSG_PICK_CHARACTER,
		MSG_PICK_ITEM,
		MSG_PICK_MAP,
		MSG_READY,
		MSG_UNREADY,
		MSG_MOVE_UP,
		MSG_MOVE_DOWN,
		MSG_PLAYER_DEAD,
		MSG_GAME_OVER,
		MSG_PLAYER_DISTANCE, // ✅ 거리 전송 메시지
		MSG_TAKEN_DAMAGE,
		MSG_END
	};
}

#pragma pack(push, 1)
struct MessageHeader
{
	int senderId;
	int msgType;
	int bodyLen;
};
#pragma pack(pop)

struct Client
{
	SOCKET sock;
	int id;
	std::thread thread;
	bool isReady = false;
	bool isAlive = true;
	int characterId = -1;
	int itemSlots[3] = { -1, -1, -1 };
	float hp = 100.0f; // ✅ 체력
	float distance = 0.0f; // ✅ 이동 거리
};

std::recursive_mutex gMutex;
std::vector<Client*> gClients;
std::unordered_set<int> gDeadPlayers;
GameState gState = WAITING;
int gNextId = 1;
int gRoomOwner = -1;
int gMapId = 0;

LARGE_INTEGER mSecond = {}, mTime = {};
float mDeltaTime = 0.f, mFPS = 0.f, mFPSTime = 0.f;
int mFPSTick = 0;

void InitTimer()
{
	QueryPerformanceFrequency(&mSecond);
	QueryPerformanceCounter(&mTime);
}

float UpdateTimer()
{
	LARGE_INTEGER Time;
	QueryPerformanceCounter(&Time);
	mDeltaTime = (Time.QuadPart - mTime.QuadPart) / (float)mSecond.QuadPart;
	mTime = Time;
	mFPSTime += mDeltaTime;
	++mFPSTick;
	if (mFPSTick == 60)
	{
		mFPS = mFPSTick / mFPSTime;
		mFPSTick = 0;
		mFPSTime = 0.f;
	}
	return mDeltaTime;
}

float GetDeltaTimeFromTimer() { return mDeltaTime; }

bool sendAll(SOCKET sock, const char* data, int len)
{
	int sent = 0;
	while (sent < len)
	{
		int r = send(sock, data + sent, len - sent, 0);
		if (r == SOCKET_ERROR) return false;
		sent += r;
	}
	return true;
}

bool sendMessage(SOCKET sock, int senderId, int msgType, const void* body, int bodyLen)
{
	MessageHeader header{ senderId, msgType, bodyLen };
	if (!sendAll(sock, (char*)&header, sizeof(header))) return false;
	if (body && bodyLen > 0) return sendAll(sock, (char*)body, bodyLen);
	return true;
}

void broadcast(int senderId, int msgType, const void* data, int len)
{
	for (auto& c : gClients)
		sendMessage(c->sock, senderId, msgType, data, len);
}

void checkGameOver()
{
	int aliveCount = 0;
	for (auto& c : gClients)
		if (c->isAlive) aliveCount++;
	if (aliveCount == 0)
	{
		gState = WAITING;
		const char* msg = "All players dead. Game over.";
		broadcast(0, (int)ServerMessage::MSG_GAME_OVER, msg, strlen(msg) + 1);
	}
}

void sendRoomFullInfo(Client* client)
{
	int playerCount = (int)gClients.size();
	int totalSize = sizeof(int) * 2 + sizeof(int) + playerCount * (sizeof(int) + sizeof(bool) + sizeof(int) * 3);
	std::vector<char> buffer(totalSize);
	char* ptr = buffer.data();
	memcpy(ptr, &gRoomOwner, sizeof(int)); ptr += sizeof(int);
	memcpy(ptr, &gMapId, sizeof(int)); ptr += sizeof(int);
	memcpy(ptr, &playerCount, sizeof(int)); ptr += sizeof(int);
	for (auto& c : gClients)
	{
		memcpy(ptr, &c->id, sizeof(int)); ptr += sizeof(int);
		memcpy(ptr, &c->isReady, sizeof(bool)); ptr += sizeof(bool);
		memcpy(ptr, c->itemSlots, sizeof(int) * 3); ptr += sizeof(int) * 3;
	}
	sendMessage(client->sock, 0, (int)ServerMessage::MSG_ROOM_FULL_INFO, buffer.data(), totalSize);
}

bool recvAll(SOCKET sock, char* buffer, int len)
{
	int recvd = 0;
	while (recvd < len)
	{
		int r = recv(sock, buffer + recvd, len - recvd, 0);
		if (r <= 0) return false;
		recvd += r;
	}
	return true;
}

bool receiveMessage(SOCKET sock, MessageHeader& header, std::vector<char>& body)
{
	if (!recvAll(sock, (char*)&header, sizeof(header))) return false;
	body.resize(header.bodyLen);
	if (header.bodyLen > 0) return recvAll(sock, body.data(), header.bodyLen);
	return true;
}

void distanceUpdateLoop()
{
	InitTimer();
	float accumulated = 0.0f;
	const float targetDelta = 1.0f / 60.0f; // 60 FPS 고정 간격

	while (true)
	{
		Sleep(1); // CPU 점유율 조절용
		std::lock_guard<std::recursive_mutex> lock(gMutex);
		if (gState != RUNNING) continue;

		float dt = UpdateTimer();
		accumulated += dt;

		while (accumulated >= targetDelta)
		{
			accumulated -= targetDelta;

			const float speed = 100.0f;
			const float hpDecayRate = 10.0f;

			for (auto& c : gClients)
			{
				if (!c->isAlive) continue;

				// 거리 & HP 갱신
				c->distance += speed * targetDelta;
				c->hp -= hpDecayRate * targetDelta;

				// 사망 처리
				if (c->hp <= 0.0f)
				{
					c->hp = 0.0f;
					c->isAlive = false;
					gDeadPlayers.insert(c->id);
					broadcast(c->id, (int)ServerMessage::MSG_PLAYER_DEAD, nullptr, 0);
					checkGameOver();
					continue;
				}

				// 거리 브로드캐스트
				struct { int id; float dist; } packetDist{ c->id, c->distance };
				broadcast(c->id, (int)ServerMessage::MSG_PLAYER_DISTANCE, &packetDist, sizeof(packetDist));

				// HP 브로드캐스트
				struct { int id; float hp; } packetHp{ c->id, c->hp };
				broadcast(c->id, (int)ServerMessage::MSG_TAKEN_DAMAGE, &packetHp, sizeof(packetHp));
			}
		}
	}
}


void clientThread(Client* client)
{
	while (true)
	{
		MessageHeader header;
		std::vector<char> body;
		if (!receiveMessage(client->sock, header, body)) break;
		std::lock_guard<std::recursive_mutex> lock(gMutex);
		switch ((ClientMessage::Type)header.msgType)
		{
		case ClientMessage::MSG_HEARTBEAT:
			sendMessage(client->sock, client->id, (int)ServerMessage::MSG_HEARTBEAT_ACK, nullptr, 0);
			break;
		case ClientMessage::MSG_START:
			if (client->id == gRoomOwner)
			{
				bool allReady = std::all_of(gClients.begin(), gClients.end(), [](Client* c)
					{
						return (c->id == gRoomOwner) || c->isReady;
					});
				if (allReady)
				{
					gState = RUNNING;
					gDeadPlayers.clear();
					for (auto& c : gClients)
					{
						c->isAlive = true;
						c->hp = 100.0f;
						c->distance = 0.0f;
					}
					const char* msg = "Game Started!";
					broadcast(client->id, (int)ServerMessage::MSG_START_ACK, msg, strlen(msg) + 1);
				}
				else
				{
					const char* errorMsg = "Not all players are ready.";
					sendMessage(client->sock, client->id, (int)ServerMessage::MSG_START_ACK, errorMsg, strlen(errorMsg) + 1);
				}
			}
			break;
		case ClientMessage::MSG_READY:
			client->isReady = true;
			broadcast(client->id, (int)ServerMessage::MSG_READY, nullptr, 0);
			break;
		case ClientMessage::MSG_UNREADY:
			client->isReady = false;
			broadcast(client->id, (int)ServerMessage::MSG_UNREADY, nullptr, 0);
			break;
		case ClientMessage::MSG_PICK_CHARACTER:
			if (header.bodyLen == sizeof(int))
			{
				memcpy(&client->characterId, body.data(), sizeof(int));
				broadcast(client->id, (int)ServerMessage::MSG_PICK_CHARACTER, body.data(), sizeof(int));
			}
			break;
		case ClientMessage::MSG_PICK_ITEM:
			if (header.bodyLen == sizeof(int) * 2)
			{
				int slot, itemId;
				memcpy(&slot, body.data(), sizeof(int));
				memcpy(&itemId, body.data() + sizeof(int), sizeof(int));
				if (slot >= 0 && slot < 3) client->itemSlots[slot] = itemId;
				broadcast(client->id, (int)ServerMessage::MSG_PICK_ITEM, body.data(), sizeof(int) * 2);
			}
			break;
		case ClientMessage::MSG_PICK_MAP:
			if (client->id == gRoomOwner && header.bodyLen == sizeof(int))
			{
				memcpy(&gMapId, body.data(), sizeof(int));
				broadcast(0, (int)ServerMessage::MSG_PICK_MAP, &gMapId, sizeof(int));
			}
			break;
		case ClientMessage::MSG_MOVE_UP:
			broadcast(client->id, (int)ServerMessage::MSG_MOVE_UP, nullptr, 0);
			break;
		case ClientMessage::MSG_MOVE_DOWN:
			broadcast(client->id, (int)ServerMessage::MSG_MOVE_DOWN, nullptr, 0);
			break;
		case ClientMessage::MSG_PLAYER_DEAD:
			client->isAlive = false;
			broadcast(client->id, (int)ServerMessage::MSG_PLAYER_DEAD, nullptr, 0);
			gDeadPlayers.insert(client->id);
			checkGameOver();
			break;
		case ClientMessage::MSG_TAKE_DAMAGE:
			if (header.bodyLen == sizeof(float))
			{
				float dmg;
				memcpy(&dmg, body.data(), sizeof(float));
				client->hp -= dmg;

				struct { int id; float hp; } packetHp{ client->id, client->hp };
				broadcast(client->id, (int)ServerMessage::MSG_TAKEN_DAMAGE, &packetHp, sizeof(packetHp));

				if (client->isAlive && client->hp <= 0.0f)
				{
					client->hp = 0.0f;
					client->isAlive = false;
					gDeadPlayers.insert(client->id);
					broadcast(client->id, (int)ServerMessage::MSG_PLAYER_DEAD, nullptr, 0);
					checkGameOver();
				}
			}
			break;
		default:
			break;
		}
	}
	{
		std::lock_guard<std::recursive_mutex> lock(gMutex);
		auto it = std::find_if(gClients.begin(), gClients.end(), [client](Client* c) { return c->id == client->id; });
		if (it != gClients.end())
		{
			bool wasOwner = (client->id == gRoomOwner);
			gClients.erase(it);
			broadcast(client->id, (int)ServerMessage::MSG_DISCONNECT, &client->id, sizeof(int));
			if (gClients.empty()) gRoomOwner = -1;
			else if (wasOwner)
			{
				gRoomOwner = gClients.front()->id;
				broadcast(0, (int)ServerMessage::MSG_NEW_OWNER, &gRoomOwner, sizeof(int));
			}
		}
	}
	closesocket(client->sock);
	delete client;
}

void LoadGameData();

int main()
{
	LoadGameData();

	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	std::thread(distanceUpdateLoop).detach();
	SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(PORT);
	bind(server, (sockaddr*)&addr, sizeof(addr));
	listen(server, SOMAXCONN);
	std::cout << "[Server] Listening on port " << PORT << "...\n";

	while (true)
	{
		sockaddr_in clientAddr{};
		int size = sizeof(clientAddr);
		SOCKET clientSock = accept(server, (sockaddr*)&clientAddr, &size);
		std::lock_guard<std::recursive_mutex> lock(gMutex);
		if ((int)gClients.size() >= MAX_PLAYERS)
		{
			const char* msg = "Room is full.";
			sendMessage(clientSock, 0, (int)ServerMessage::MSG_CONNECTED_REJECT, msg, strlen(msg) + 1);
			closesocket(clientSock);
			continue;
		}
		Client* c = new Client;
		c->sock = clientSock;
		c->id = gNextId++;
		gClients.push_back(c);
		if (gRoomOwner == -1) gRoomOwner = c->id;
		sendMessage(clientSock, c->id, (int)ServerMessage::MSG_CONNECTED, &c->id, sizeof(int));
		sendRoomFullInfo(c);
		for (auto& other : gClients)
		{
			if (other->id != c->id)
				sendMessage(other->sock, c->id, (int)ServerMessage::MSG_JOIN, &c->id, sizeof(int));
		}
		c->thread = std::thread(clientThread, c);
		c->thread.detach();
	}
	closesocket(server);
	WSACleanup();
	return 0;
}

void LoadGameData()
{
	// config load
	std::string webserverPath = WEBSERVER_PATH;
	std::string path = webserverPath + CONFIG_PATH;
	std::string configResult = CCURL::GetInst()->SendRequest(path, METHOD_GET);
	printf(("configResult: " + configResult).c_str());
	CDataStorageManager::GetInst()->SetConfigData(configResult);

	// characters load
	path = webserverPath + CDataStorageManager::GetInst()->GetConfig().CharacterFileName;
	std::string charactersResult = CCURL::GetInst()->SendRequest(path, METHOD_GET);
	printf(("charactersResult: " + charactersResult).c_str());
	CDataStorageManager::GetInst()->SetCharacterData(charactersResult);

	// maps load
	for (std::string mapFileName : CDataStorageManager::GetInst()->GetConfig().mapFileNameList)
	{
		path = webserverPath + mapFileName;
		std::string mapResult = CCURL::GetInst()->SendRequest(path, METHOD_GET);
		printf(("mapResult: " + mapResult).c_str());
		CDataStorageManager::GetInst()->SetMapData(mapResult);
	}

	// stat load
	path = webserverPath + CDataStorageManager::GetInst()->GetConfig().StatFileName;
	std::string statsResult = CCURL::GetInst()->SendRequest(path, METHOD_GET);
	printf(("statsResult: " + statsResult).c_str());
	CDataStorageManager::GetInst()->SetStatInfoData(charactersResult);

	// item load
	path = webserverPath + CDataStorageManager::GetInst()->GetConfig().ItemFileName;
	std::string itemResult = CCURL::GetInst()->SendRequest(path, METHOD_GET);
	printf(("itemResult: " + itemResult).c_str());
	CDataStorageManager::GetInst()->SetItemInfoData(itemResult);
}
