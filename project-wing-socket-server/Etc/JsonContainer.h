#pragma once

#include "GameInfo.h"

struct FUserInfo
{
	std::string Name = "";
	std::string Map = "";
	int Distance = 0; // Rank
	int Time = 0;

	FUserInfo() {};
	FUserInfo(std::string name, std::string map, int dist, int time)
		: Name(name), Map(map), Distance(dist), Time(time) {};
};

/*
	"top_y_pos": 720.0,
	"bottom_y_pos": 0.0,
	"item_type": -1,
	"obstacle_type": -1
*/
struct FLineNode
{
	float TopYPos = 0.0f;
	float BottomYPos = 0.0f;
	int ItemType = 0;
	int ObstacleType = 0;

	FLineNode() {};
	FLineNode(float topYPos, float bottomYPos, int itemType, int obstacleType)
		:TopYPos(topYPos), BottomYPos(bottomYPos)
		, ItemType(itemType), ObstacleType(obstacleType) {};
};

/*
{
	"index": 0,
	"name": "Easy",
	"difficulty_color_name": "Cyan",
	"difficulty_rate": 10.0,
	"collision_damage": 20.0,
	"line_node_list": [
		{
			"top_y_pos": 720.0,
			"bottom_y_pos": 0.0,
			"item_type": -1,
			"obstacle_type": -1
		},
*/
struct FMapInfo
{
	int Index;
	std::string Name;
	std::string DifficultyColorName;
	float DifficultyRate;
	float CollisionDamage;
	std::vector<FLineNode> lineNodes;
};


/*
{
	"character_list": [
		{
			"index": 0,
			"name": "Yang",
			"color_name": "White",
			"speed": 700.0,
			"hp": 100.0,
			"dex": 100.0,
			"def": 10.0,
			"stun_duration": 2.0,
			"image_sequence_name": "PlayerIdle",
			"size_x": 200.0,
			"size_y": 135.0
		},
		...
*/
struct FCharacterState
{
	int Index;
	std::string Name;
	std::string ColorName;
	float Speed;
	float HP;
	float Dex;
	float Def;
	float StunDuration;
	std::string ImageSequenceName;
	float SizeX;
	float SizeY;
};

/*
{
	"db_id": "19e45759635e8028adb0d83e3cf969ff",
	"db_url": "https://api.notion.com/v1/databases/19e45759635e8028adb0d83e3cf969ff",
	"api_key": "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",
	"map_file_list": [
		"map_0.json",
		"map_1.json",
		"map_2.json"
	],
	"character_file": "character_list.json",
	"item_file": "item_list.json",
	"stat_file": "stat_type_list.json",
	"selectable_item_count": 3
}
*/
struct FConfig
{
	std::string DatabaseID;
	std::string DatabaseURL;
	std::string APIKey;
	std::vector<std::string> mapFileNameList;
	std::string CharacterFileName;
	std::string ItemFileName;
	std::string StatFileName;
	int SelectableItemCount;
};


/*
{
	"fileName": "sprite_sheet_character",
	"prefix": "attack_down_",
	"sizeX": 600.0,
	"sizeY": 187.0,
	"sprites": [
		{
			"name": "attack_down_0",
			"x": 315.0,
			"y": 144.0,
			"width": 32.0,
			"height": 42.0,
			"pivotX": 0.5,
			"pivotY": 0.0
		},
*/
struct FSpriteSheetInfo
{
	std::string Name;
	float X;
	float Y;
	float Width;
	float Height;
	float PivotX;
	float PivotY;
};

struct FSpriteAtlasInfo
{
	std::string FileName;
	std::string Prefix;
	float SizeX;
	float SizeY;
	std::vector<FSpriteSheetInfo> Sprites;
};

namespace EStatInfo
{
	enum Type
	{
		HP = 0,
		Speed,
		Dex,
		Def,
		End
	};
}

/*
{
	"stat_type_list": [
		{
			"index": 0,
			"type": 0,
			"name": "Hp"
		},
*/
struct FStatInfo
{
	int Index;
	int Type;
	std::string Name;
};

/*
{
	"item_list": [
		{
			"index": 0,
			"name": "Potion",
			"stat_type": 0,
			"add_value": 50.0,
			"desc": "When held, maximum stamina increases."
		},
*/
struct FItemInfo
{
	int Index;
	std::string Name;
	int StatType;
	float AddValue;
	std::string Desc;
};

