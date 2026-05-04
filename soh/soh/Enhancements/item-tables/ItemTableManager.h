#pragma once
#include "ItemTableTypes.h"
#include "z64item.h"

#include <unordered_map>

typedef std::unordered_map<uint16_t, GetItemEntry> ItemTable;

class ItemTableManager {
  public:
    static ItemTableManager* Instance;
    ItemTableManager();
    ~ItemTableManager();
    bool AddItemTable(uint16_t tableID);
    bool AddItemEntry(uint16_t tableID, uint16_t getItemID, GetItemEntry getItemEntry);
    // Like AddItemEntry, but overwrites an existing entry under the same key
    // (AddItemEntry uses emplace and silently fails on collision). Used by
    // mods that want to claim slots reserved as GET_ITEM_NONE in vanilla.
    bool SetItemEntry(uint16_t tableID, uint16_t getItemID, GetItemEntry getItemEntry);
    GetItemEntry RetrieveItemEntry(uint16_t tableID, uint16_t getItemID);
    bool ClearItemTable(uint16_t tableID);

  private:
    std::unordered_map<uint16_t, ItemTable> itemTables;

    ItemTable* RetrieveItemTable(uint16_t tableID);
};
