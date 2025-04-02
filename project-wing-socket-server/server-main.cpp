#pragma once
#include "GameInfo.h"
#include "Etc/JsonContainer.h"
#include "Etc/CURL.h"
#include "Etc/DataStorageManager.h"
#include "Etc/JsonController.h"
#include "Interface/IPlayerStatController.h"

#define PORT 12345
#define MAX_PLAYERS 5
#define SCREEN_WIDTH 1280.0f
#define SCREEN_HEIGHT 720.0f

//std::cout << "client_" << c->id
				//	<< "  " <<
				//	<< "\n";

// 높이 가운데서부터 시작 함.
#define PLAYER_INIT_POS_HEIGHT 0.0f

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
		MSG_TAKE_DAMAGE, // 맵에 박았을때의 트리거
		MSG_BOOST_ON,
		MSG_BOOST_OFF
	};
}

namespace ServerMessage
{
	enum Type
	{
		MSG_CONNECTED,
		MSG_ROOM_FULL_INFO,
		MSG_DISCONNECT, // 이건 누가 나간거.
		MSG_CONNECTED_REJECT,
		MSG_NEW_OWNER,
		MSG_JOIN,

		MSG_PICK_MAP,
		MSG_PICK_ITEM,
		MSG_PICK_CHARACTER,
		MSG_READY,
		MSG_UNREADY,
		MSG_START_ACK,

		MSG_COUNTDOWN_FINISHED,
		MSG_PLAYER_DEAD,
		MSG_GAME_OVER,
		MSG_MOVE_UP,
		MSG_MOVE_DOWN,
		MSG_PLAYER_DISTANCE, // 거리 전송 메시지.
		MSG_PLAYER_HEIGHT,
		MSG_TAKEN_DAMAGE,	// 현재 HP 알려줌.
		MSG_TAKEN_STUN,
		MSG_BOOST_ON,
		MSG_BOOST_OFF,

		MSG_HEARTBEAT_ACK,
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

struct Client : public IPlayerStatController
{
	SOCKET sock;
	int id;
	std::thread thread;

	bool isReady = false;
	bool isAlive = true;
	bool isMovingUp = false;

	int characterId = 0;
	int itemSlots[3] = { -1, -1, -1 };

	float height = 0.0f;

	void Init()
	{
		isReady = false;
		isAlive = true;
		isMovingUp = false;
		characterId = 0;
		itemSlots[0] = -1;
		itemSlots[1] = -1;
		itemSlots[2] = -1;
		height = 0.0f;
	}
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
	{
		if (c->isAlive)
			aliveCount++;
	}

	if (aliveCount == 0)
	{
		for (auto& c : gClients)
			c->Init();

		gState = WAITING;
		const char* msg = "All players dead. Game over.";
		broadcast(0, (int)ServerMessage::MSG_GAME_OVER, msg, strlen(msg) + 1);
	}
}

void sendRoomFullInfo(Client* client)
{
	int playerCount = (int)gClients.size();
	int totalSize = sizeof(int)  // roomOwner
		+ sizeof(int) // mapId
		+ sizeof(int) // playerCount
		+ playerCount // 인원수 곱
		* (sizeof(int) // id
			+ sizeof(bool)  // ready
			+ sizeof(int)  // character
			+ sizeof(int) * 3); // item*3

	std::vector<char> buffer(totalSize);
	char* ptr = buffer.data();
	memcpy(ptr, &gRoomOwner, sizeof(int)); ptr += sizeof(int);
	memcpy(ptr, &gMapId, sizeof(int)); ptr += sizeof(int);
	memcpy(ptr, &playerCount, sizeof(int)); ptr += sizeof(int);
	for (auto& c : gClients)
	{
		memcpy(ptr, &c->id, sizeof(int)); ptr += sizeof(int);
		memcpy(ptr, &c->isReady, sizeof(bool)); ptr += sizeof(bool);
		memcpy(ptr, &c->characterId, sizeof(int)); ptr += sizeof(int);
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

void InGameUpdateLoop()
{
	InitTimer();
	float broadcastAccumulated = 0.0f;
	const float targetDelta = 1.0f / 30.0f;
	const float countDownTime = 5.0f;
	float curCountDownTime = 0.0f;
	bool isFinishCountDown = false;

	while (true)
	{
		Sleep(1);
		std::lock_guard<std::recursive_mutex> lock(gMutex);
		float dt = UpdateTimer(); // 이번 프레임 시간

		if (gState != RUNNING)
		{
			isFinishCountDown = false;
			continue;
		}

		broadcastAccumulated += dt;

		// 카운트다운 처리
		if (!isFinishCountDown)
		{
			curCountDownTime += dt;

			//std::cout
			//	<< " curCountDownTime " << curCountDownTime
			//	<< "\n";

			if (curCountDownTime < countDownTime)
				continue;
			else
			{
				isFinishCountDown = true;
				curCountDownTime = 0.0f;
				broadcast(0, (int)ServerMessage::MSG_COUNTDOWN_FINISHED, nullptr, 0);
			}
		}

		// 🧠 스탯 업데이트는 매 프레임 처리
		for (auto& c : gClients)
		{
			if (!c->isAlive) continue;

			if (c->GetIsProtection())
			{
				c->ReleaseProtection(dt);
			}

			if (c->GetIsStun())
			{
				c->ReleaseStun(dt);
			}

			//std::cout << "client_" << c->id
			//	<< " c->GetIsStun(): " << c->GetIsStun() << "\n";

			if (!c->GetIsStun())
			{
				float _height = c->GetDex() * dt * (c->isMovingUp ? 1.0f : -1.0f);
				c->height += _height;
				c->height = clamp(c->height, SCREEN_HEIGHT * -0.5f, SCREEN_HEIGHT * 0.5f);

				//std::cout << "client_" << c->id
				//	<< " c->isMovingUp: " << c->isMovingUp
				//	<< " c->GetDex(): " << c->GetDex()
				//	<< " dt: " << dt
				//	<< " _height: " << _height
				//	<< " c->height: " << c->height << "\n";

				// 거리 & HP 갱신
				float speed = c->GetSpeed();
				float boostMultiplyValue = c->GetBoostValue();
				float speedPerFrame = speed * dt * 0.01f * boostMultiplyValue;
				c->AddPlayDistance(speedPerFrame);
			}

			if (!c->GetIsStun() && !c->GetIsProtection())
			{
#ifdef _DEBUG
				c->DamagedPerDistance(dt * 10.0f);
#else
				c->DamagedPerDistance(dt);
#endif
				if (c->isAlive && c->GetCurHP() <= 0.0f)
				{
					std::cout << "DamagedPerDistance Dead id: " << c->id << "\n";
					c->isAlive = false;
					gDeadPlayers.insert(c->id);
					broadcast(c->id, (int)ServerMessage::MSG_PLAYER_DEAD, nullptr, 0);
					checkGameOver();
				}
			}
		}

		// 💬 60FPS 기준으로 메시지 브로드캐스트
		while (broadcastAccumulated >= targetDelta)
		{
			broadcastAccumulated -= targetDelta;

			for (auto& c : gClients)
			{
				if (!c->isAlive) continue;

				// 죽은 캐릭이라도 계속 보내야 함.
				float _dist = c->GetPlayDistance();
				float _height = c->height;
				float _curHp = c->GetCurHP();

				std::cout << "client_" << c->id
					<< " _dist: " << _dist
					<< " _height: " << _height
					<< " _curHp: " << _curHp << "\n";

				broadcast(c->id, (int)ServerMessage::MSG_PLAYER_DISTANCE, &_dist, sizeof(float));
				broadcast(c->id, (int)ServerMessage::MSG_PLAYER_HEIGHT, &_height, sizeof(float));
				broadcast(c->id, (int)ServerMessage::MSG_TAKEN_DAMAGE, &_curHp, sizeof(float));
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
				bool allReady = std::all_of(gClients.begin(), gClients.end(),
					[](Client* c)
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

						// 스탯 계산해서 Init 하기.
						// 테이블 읽어서 기본 스텟 초기화.
						auto _statInfo = CDataStorageManager::GetInst()->GetCharacterState(c->characterId);

						std::cout << "client_" << c->id
							<< " _statInfo.HP: " << _statInfo.HP
							<< " _statInfo.Speed: " << _statInfo.Speed
							<< " _statInfo.Dex: " << _statInfo.Dex
							<< " _statInfo.Def: " << _statInfo.Def
							<< "\n";

						c->InitStat(_statInfo);

						std::cout << "client_" << c->id
							<< " c->GetHP: " << c->GetCurHP()
							<< " c->GetSpeed: " << c->GetSpeed()
							<< " c->GetDex: " << c->GetDex()
							<< " c->GetDef: " << c->GetDef()
							<< "\n";

						// 착용한 아이템 스텟에 적용.
						auto _itemDatas = CDataStorageManager::GetInst()->GetItemInfoDatas();
						int _itemLength = sizeof(c->itemSlots) / sizeof(c->itemSlots[0]);
						for (int i = 0; i < _itemLength; i++)
						{
							int _itemIndexInSlot = c->itemSlots[i];
							if (_itemIndexInSlot >= 0)
							{
								// 어떤 스탯에 얼마를 적용할것인지.
								c->AddValueByStatIndex(
									static_cast<EStatInfo::Type>(_itemDatas[_itemIndexInSlot].StatType)
									, _itemDatas[_itemIndexInSlot].AddValue);
							}
						}
					}

				}
				
				// 시작에 대한 결과를 알려줘야 함.
				int readyFlag = static_cast<int>(allReady);
				broadcast(client->id, (int)ServerMessage::MSG_START_ACK, &readyFlag, sizeof(int));
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
				CDataStorageManager::GetInst()->SetSelectedMapIndex(gMapId);
				broadcast(0, (int)ServerMessage::MSG_PICK_MAP, &gMapId, sizeof(int));
			}
			break;
		case ClientMessage::MSG_MOVE_UP:
			client->isMovingUp = true;
			broadcast(client->id, (int)ServerMessage::MSG_MOVE_UP, nullptr, 0);
			//std::cout << "ClientMessage::MSG_MOVE_UP id: " << client->id << "\n";
			break;
		case ClientMessage::MSG_MOVE_DOWN:
			client->isMovingUp = false;
			broadcast(client->id, (int)ServerMessage::MSG_MOVE_DOWN, nullptr, 0);
			//std::cout << "ClientMessage::MSG_MOVE_DOWN id: " << client->id << "\n";
			break;
		case ClientMessage::MSG_TAKE_DAMAGE:
			if (header.bodyLen == sizeof(float))
			{
				std::cout << "ClientMessage::MSG_TAKE_DAMAGE id: " << client->id << "\n";
				// 맵 테이블에 의한 데이지.
				float _damage = CDataStorageManager::GetInst()->GetSelectedMapInfo().CollisionDamage;
				client->SetStun();
				broadcast(client->id, (int)ServerMessage::MSG_TAKEN_STUN, nullptr, 0);
				client->Damaged(_damage);

				struct { int id; float hp; } packetHp{ client->id, client->GetCurHP() };
				broadcast(client->id, (int)ServerMessage::MSG_TAKEN_DAMAGE, &packetHp, sizeof(packetHp));

				if (client->isAlive && client->GetCurHP() <= 0.0f)
				{
					std::cout << "ClientMessage::MSG_TAKE_DAMAGE Dead######## id: " << client->id << "\n";
					client->isAlive = false;
					gDeadPlayers.insert(client->id);
					broadcast(client->id, (int)ServerMessage::MSG_PLAYER_DEAD, nullptr, 0);
					checkGameOver();
				}
			}
			break;
		case ClientMessage::MSG_BOOST_ON:
			client->SetIsBoostMode(true);
			broadcast(client->id, (int)ServerMessage::MSG_BOOST_ON, nullptr, 0);
			break;
		case ClientMessage::MSG_BOOST_OFF:
			client->SetIsBoostMode(false);
			broadcast(client->id, (int)ServerMessage::MSG_BOOST_OFF, nullptr, 0);
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
	CDataStorageManager::GetInst()->SetStatInfoData(statsResult);

	// item load
	path = webserverPath + CDataStorageManager::GetInst()->GetConfig().ItemFileName;
	std::string itemResult = CCURL::GetInst()->SendRequest(path, METHOD_GET);
	printf(("itemResult: " + itemResult).c_str());
	CDataStorageManager::GetInst()->SetItemInfoData(itemResult);
}

int main()
{
	LoadGameData();

	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	std::thread(InGameUpdateLoop).detach();
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
		c->Init();
		std::cout << "[Server] gClients.push_back(c); " << c->id << "\n";
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

