#include "Definitions.h"
#include "Items.h"
#include "Textures.h"
itemInfo itemsinfo[] = { STICK, APPLE };
void loadItemsTextures()
{
	itemsinfo[builtInItems::STICK - theFirstItem].texture =
	Textures::LoadRGBTexture("Textures/items/stick.bmp");
	itemsinfo[builtInItems::APPLE - theFirstItem].texture =
	Textures::LoadRGBTexture("Textures/items/apple.bmp");
}
