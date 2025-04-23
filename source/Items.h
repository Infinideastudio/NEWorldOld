#pragma once
#include "Definitions.h"

class ItemInfo {
public:
	ItemInfo(ItemID itemid, TextureID itemtexture=0) :id(itemid), texture(itemtexture) {}
	ItemID id;
	TextureID texture;
};

enum BuiltInItems {
	STICK = 30000, APPLE
};

extern ItemInfo itemsinfo[];
const ItemID theFirstItem = STICK;

void loadItemsTextures();

inline bool isBlock(ItemID i) {
	return i < theFirstItem;
}

inline TextureID getItemTexture(ItemID i){
	if (isBlock(i)) return BlockTextures;
	else return itemsinfo[i - theFirstItem].texture;
}