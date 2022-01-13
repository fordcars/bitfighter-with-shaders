//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _UIQUICKCHAT_H_
#define _UIQUICKCHAT_H_


#include "helperMenu.h"

#include "tnlNetBase.h"
#include "tnlNetStringTable.h"

#include <string>

using namespace std;

namespace Zap
{

struct QuickChatNode
{
   U32 depth;
   InputCode inputCode;
   InputCode buttonCode;
   bool teamOnly;
   bool commandOnly;
   string caption;
   string msg;
   bool isMsgItem;         // False for groups, true for messages

   QuickChatNode();        // Constructor
   virtual ~QuickChatNode();
};


////////////////////////////////////////
////////////////////////////////////////

class QuickChatHelper : public HelperMenu
{
   typedef HelperMenu Parent;

private:
   S32 mCurNode;
   Vector<OverlayMenuItem> mMenuItems1;
   Vector<OverlayMenuItem> mMenuItems2;
   bool mMenuItems1IsCurrent;

   const S32 mQuickChatItemsDisplayWidth;
   S32 mQuickChatButtonsWidth;

   Vector<OverlayMenuItem> *getMenuItems(bool one);
   S32 getWidthOfItems() const;
   S32 getWidthOfButtons() const;

   void updateChatMenuItems(S32 curNode);

public:
   explicit QuickChatHelper();      // Constructor
   virtual ~QuickChatHelper();

   HelperMenuType getType();

   static Vector<QuickChatNode> nodeTree;

   void render();                
   void onActivated();  
   bool processInputCode(InputCode inputCode);   
   bool isMovementDisabled() const;
};

extern Vector<QuickChatNode> gQuickChatTree;      // Holds our tree of QuickChat groups and messages, as defined in the INI file

};

#endif

