//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "EditorAttributeMenuItemBuilder.h"
#include "BfObject.h"
#include "UIEditorMenus.h"
#include "moveObject.h"
#include "CoreGame.h"      // For CoreItem static values
#include "EngineeredItem.h"
#include "Spawn.h"
#include "PickupItem.h"
#include "TextItem.h"
#include "Teleporter.h"

#include "stringUtils.h"

namespace Zap
{


EditorAttributeMenuItemBuilder::EditorAttributeMenuItemBuilder()
{
   mInitialized = false;
   mGame = NULL;
}

// Destructor
EditorAttributeMenuItemBuilder::~EditorAttributeMenuItemBuilder()
{
   // Do nothing
}


void EditorAttributeMenuItemBuilder::initialize(ClientGame *game)
{
   mGame = game;
   mInitialized = true;
}


// Since many of these attribute menus will never be shown in a given session, and each is relatively inexpensive to build,
// we'll create them lazily on an as-needed basis.  Each section below has a static pointer enclosed in a block so that it
// will be isloated from other similar variables with the same name.  Since these are statics, they will be destroyed only
// when this object is destroyed, which will be when the game exits.

EditorAttributeMenuUI *EditorAttributeMenuItemBuilder::getAttributeMenu(BfObject *obj)
{
   TNLAssert(mInitialized, "Must initialize before use!");

   switch(obj->getObjectTypeNumber())
   {
      case AsteroidTypeNumber:
      {
         static EditorAttributeMenuUI *attributeMenuUI = NULL;

         if(!attributeMenuUI)
         {
            attributeMenuUI = new EditorAttributeMenuUI(mGame);

            attributeMenuUI->addMenuItem(
                  new CounterMenuItem("Size:", Asteroid::ASTEROID_INITIAL_SIZELEFT, 1, 1, Asteroid::ASTEROID_SIZELEFT_MAX, "", "", "")
               );

            // Add our standard save and exit option to the menu
            attributeMenuUI->addSaveAndQuitMenuItem();
         }

         return attributeMenuUI;
      }

      case ShipSpawnTypeNumber:  // Nothing for Ship Spawn
         return NULL;

      case AsteroidSpawnTypeNumber:
      case FlagSpawnTypeNumber:
      {
         static EditorAttributeMenuUI *attributeMenuUI = NULL;

         if(!attributeMenuUI)
         {
            ClientGame *clientGame = static_cast<ClientGame *>(mGame);

            attributeMenuUI = new EditorAttributeMenuUI(clientGame);

            CounterMenuItem *menuItem = new CounterMenuItem("Spawn Timer:", 999, 1, 0, 1000, "secs", "Never spawns", 
                                                            "Time it takes for each item to be spawned");
            attributeMenuUI->addMenuItem(menuItem);

            // Additionally add the asterod size
            if(obj->getObjectTypeNumber() == AsteroidSpawnTypeNumber)
            {
               attributeMenuUI->addMenuItem(
                     new CounterMenuItem("Asteroid Size:", Asteroid::ASTEROID_INITIAL_SIZELEFT, 1, 1, Asteroid::ASTEROID_SIZELEFT_MAX, "", "", "")
               );
            }

            // Add our standard save and exit option to the menu
            attributeMenuUI->addSaveAndQuitMenuItem();
         }

         return attributeMenuUI;
      }

      case CoreTypeNumber:
      {
         static EditorAttributeMenuUI *attributeMenuUI = NULL;

         if(!attributeMenuUI)
         {
            ClientGame *clientGame = static_cast<ClientGame *>(mGame);

            attributeMenuUI = new EditorAttributeMenuUI(clientGame);

            attributeMenuUI->addMenuItem(new CounterMenuItem("Hit points:", CoreItem::CoreDefaultStartingHealth,
                                         1, 1, S32(CoreItem::DamageReductionRatio), "", "", ""));

            attributeMenuUI->addMenuItem(new CounterMenuItem("Rotation speed:", CoreItem::CoreDefaultRotationSpeed,
                                         1, 0, CoreItem::CoreMaxRotationSpeed, "x", "Stopped", ""));

            // Add our standard save and exit option to the menu
            attributeMenuUI->addSaveAndQuitMenuItem();
         }

         return attributeMenuUI;
      }


      case ForceFieldProjectorTypeNumber:
      {
         static EditorAttributeMenuUI *attributeMenuUI = NULL;

         if(!attributeMenuUI)
         {
            ClientGame *clientGame = static_cast<ClientGame *>(mGame);

            attributeMenuUI = new EditorAttributeMenuUI(clientGame);

            // Value doesn't matter (set to 99 here), as it will be clobbered when startEditingAttrs() is called
            CounterMenuItem *menuItem = new CounterMenuItem("10% Heal:", 99, 1, 0, 100, "secs", "Disabled", 
                                                            "Time for this item to heal itself 10%");
            attributeMenuUI->addMenuItem(menuItem);

            // Add our standard save and exit option to the menu
            attributeMenuUI->addSaveAndQuitMenuItem();
         }

         return attributeMenuUI;
      }


      case RepairItemTypeNumber:
      case EnergyItemTypeNumber:
      {
         static EditorAttributeMenuUI *attributeMenuUI = NULL;

         if(attributeMenuUI == NULL)
         {
            ClientGame *clientGame = static_cast<ClientGame *>(mGame);

            attributeMenuUI = new EditorAttributeMenuUI(clientGame);

            // Value doesn't matter (set to 99 here), as it will be clobbered when startEditingAttrs() is called
            CounterMenuItem *menuItem = new CounterMenuItem("Regen Time:", 99, 1, 0, 100, "secs", "No regen", 
                                                            "Time for this item to reappear after it has been picked up");

            attributeMenuUI->addMenuItem(menuItem);

            // Add our standard save and exit option to the menu
            attributeMenuUI->addSaveAndQuitMenuItem();
         }

         return attributeMenuUI;
      }
   
      case TextItemTypeNumber:
      {
         static EditorAttributeMenuUI *attributeMenuUI = NULL;

         // Lazily initialize this -- if we're in the game, we'll never need this to be instantiated
         if(!attributeMenuUI)
         {
            attributeMenuUI = new EditorAttributeMenuUI(static_cast<ClientGame *>(mGame));

            // "Blah" will be overwritten when startEditingAttrs() is called
            TextEntryMenuItem *menuItem = new TextEntryMenuItem("Text: ", "Blah", "", "", MAX_TEXTITEM_LEN);
            menuItem->setTextEditedCallback(TextItem::textEditedCallback);

            attributeMenuUI->addMenuItem(menuItem);

            // Add our standard save and exit option to the menu
            attributeMenuUI->addSaveAndQuitMenuItem();
         }

         return attributeMenuUI;
      }

      case TeleporterTypeNumber:
      {
         static EditorAttributeMenuUI *attributeMenuUI = NULL;

         if(attributeMenuUI == NULL)
         {
            ClientGame *clientGame = static_cast<ClientGame *>(mGame);

            attributeMenuUI = new EditorAttributeMenuUI(clientGame);

            // Values don't matter, they will be overwritten when startEditingAttrs() is called
            FloatCounterMenuItem *menuItem = new FloatCounterMenuItem("Delay:",
                  1.5f, 0.1f, 0.1f, 10000.0f, 1, "seconds", "Almost no delay",
                  "Adjust teleporter cooldown for re-entry");

            attributeMenuUI->addMenuItem(menuItem);

            // Add our standard save and exit option to the menu
            attributeMenuUI->addSaveAndQuitMenuItem();
         }

         return attributeMenuUI;
      }

      default:
         return obj->getAttributeMenu();
   }
}


// Get the menu looking like what we want (static)
void EditorAttributeMenuItemBuilder::startEditingAttrs(EditorAttributeMenuUI *attributeMenu, BfObject *obj)
{
   switch(obj->getObjectTypeNumber())
   {
      case AsteroidTypeNumber:
         attributeMenu->getMenuItem(0)->setIntValue(static_cast<Asteroid *>(obj)->getCurrentSize());
         break;

      case ShipSpawnTypeNumber:
      case AsteroidSpawnTypeNumber:
      case FlagSpawnTypeNumber:
         attributeMenu->getMenuItem(0)->setIntValue(static_cast<AbstractSpawn *>(obj)->getSpawnTime());

         if(obj->getObjectTypeNumber() == AsteroidSpawnTypeNumber)
            attributeMenu->getMenuItem(1)->setIntValue(static_cast<AsteroidSpawn *>(obj)->getAsteroidSize());

         break;

      case CoreTypeNumber:
         attributeMenu->getMenuItem(0)->setIntValue(S32(static_cast<CoreItem *>(obj)->getStartingHealth() + 0.5));
         attributeMenu->getMenuItem(1)->setIntValue(S32(static_cast<CoreItem *>(obj)->getRotationSpeed()));
         break;

      case ForceFieldProjectorTypeNumber:
         attributeMenu->getMenuItem(0)->setIntValue(static_cast<EngineeredItem *>(obj)->getHealRate());
         break;

      case RepairItemTypeNumber:
      case EnergyItemTypeNumber:
         attributeMenu->getMenuItem(0)->setIntValue(static_cast<PickupItem *>(obj)->getRepopDelay());
         break;

      case TextItemTypeNumber:
         attributeMenu->getMenuItem(0)->setValue(static_cast<TextItem *>(obj)->getText());
         break;

      case TeleporterTypeNumber:
         attributeMenu->getMenuItem(0)->setValue(Zap::ftos((static_cast<Teleporter *>(obj)->getDelay() / 1000.f), 3));
         break;

      default:
         obj->startEditingAttrs(attributeMenu);
         break;
   }
}


// Retrieve the values we need from the menu (static)
void EditorAttributeMenuItemBuilder::doneEditingAttrs(EditorAttributeMenuUI *attributeMenu, BfObject *obj)
{
   switch(obj->getObjectTypeNumber())
   {
      case AsteroidTypeNumber:
         static_cast<Asteroid *>(obj)->setCurrentSize(attributeMenu->getMenuItem(0)->getIntValue());
         break;

      case ShipSpawnTypeNumber:
      case AsteroidSpawnTypeNumber:
      case FlagSpawnTypeNumber:
         static_cast<AbstractSpawn *>(obj)->setSpawnTime(attributeMenu->getMenuItem(0)->getIntValue());

         if(obj->getObjectTypeNumber() == AsteroidSpawnTypeNumber)
            static_cast<AsteroidSpawn *>(obj)->setAsteroidSize(attributeMenu->getMenuItem(1)->getIntValue());

         break;

      case CoreTypeNumber:
         static_cast<CoreItem *>(obj)->setStartingHealth(F32(attributeMenu->getMenuItem(0)->getIntValue()));
         static_cast<CoreItem *>(obj)->setRotationSpeed(U32(attributeMenu->getMenuItem(1)->getIntValue()));
         break;
         
      case ForceFieldProjectorTypeNumber:
         static_cast<EngineeredItem *>(obj)->setHealRate(attributeMenu->getMenuItem(0)->getIntValue());
         break;

      case RepairItemTypeNumber:
      case EnergyItemTypeNumber:
         static_cast<PickupItem *>(obj)->setRepopDelay(attributeMenu->getMenuItem(0)->getIntValue());
         break;

      case TextItemTypeNumber:
         static_cast<TextItem *>(obj)->setText(attributeMenu->getMenuItem(0)->getValue());
         break;

      default:
         obj->doneEditingAttrs(attributeMenu);
         break;
   }
}


}
