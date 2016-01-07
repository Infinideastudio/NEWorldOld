#ifndef ITEMS_H
#define ITEMS_H
#include "Definitions.h"
class itemInfo {
	public:
		itemInfo(item itemid, TextureID itemtexture = 0) :id(itemid), texture(itemtexture) {}
		item id;
		TextureID texture;
};
enum builtInItems {
	STICK = 30000, APPLE
};
extern itemInfo itemsinfo[];
const item theFirstItem = STICK;
void loadItemsTextures();
inline bool isBlock(item i) {
	return i < theFirstItem;
}
inline TextureID getItemTexture(item i) {
	if (isBlock(i)) return BlockTextures;
	else return itemsinfo[i - theFirstItem].texture;
}
#endif
