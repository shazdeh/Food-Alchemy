#include "logger.h"
#include "CLibUtilsQTR/FormReader.hpp"

std::unordered_map<TESBoundObject*, TESBoundObject*> map;

bool bInAlchemyMenu;

bool IsAlchemyMenu() {
    const auto menu = UI::GetSingleton()->GetMenu<CraftingMenu>().get();
    return skyrim_cast<CraftingSubMenus::CraftingSubMenus::AlchemyMenu*>(menu->GetCraftingSubMenu());
}

void ConvertFoodToIngredient() {
    Actor* player = PlayerCharacter::GetSingleton();
    const auto& inventory = player->GetInventory();

    for (const auto& [food, ingredient] : map) {
        auto it = inventory.find(food);
        if (it != inventory.end()) {
            auto count = it->second.first;
            player->AddObjectToContainer(ingredient, nullptr, count, nullptr);
        }
    }
}

void ConvertIngredientsToFood() {
    Actor* player = PlayerCharacter::GetSingleton();
    const auto& inventory = player->GetInventory();

    for (const auto& [food, ingredient] : map) {
        // remove unused ingredients we added in ConvertFoodToIngredient()
        auto invIng = inventory.find(ingredient);
        auto ingCount = 0;
        if (invIng != inventory.end()) {
            ingCount = invIng->second.first;
            player->RemoveItem(invIng->first, ingCount, ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        }

        auto it = inventory.find(food);
        if (it != inventory.end()) {
            auto count = it->second.first;
            if (count - ingCount > 0) { // we used up a food item?
                player->RemoveItem(it->first, count - ingCount, ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            }
        }
    }
}

struct EventSink : public BSTEventSink<MenuOpenCloseEvent> {
    BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent* event, BSTEventSource<MenuOpenCloseEvent>*) {
        if (event->menuName != CraftingMenu::MENU_NAME) return BSEventNotifyControl::kContinue;
        if (event->opening && IsAlchemyMenu()) {
            bInAlchemyMenu = true;
            ConvertFoodToIngredient();
        } else if (bInAlchemyMenu) {
            bInAlchemyMenu = false;
            ConvertIngredientsToFood();
        }
        return BSEventNotifyControl::kContinue;
    }
};

void CompileItemsList() {
    const std::filesystem::path filePath = "Data/SKSE/Plugins/FoodAlchemy.txt";
    if (!std::filesystem::exists(filePath)) {
        return;
    }
    std::ifstream file{filePath};
    if (!file.is_open()) {
        return;
    }
    std::string line;
    int i = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        const auto data = FormReader::split(line, " : ");
        if (data.size() != 2) continue;
        auto foodID = FormReader::GetFormEditorIDFromString(data[0]);
        auto ingredientID = FormReader::GetFormEditorIDFromString(data[1]);
        if (foodID && ingredientID) {
            auto food = TESForm::LookupByID<TESBoundObject>(foodID);
            auto ingredient = TESForm::LookupByID<TESBoundObject>(ingredientID);
            if (food && ingredient) {
                map.insert({food, ingredient});
            }
        }
    }
}

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        CompileItemsList();
        if (!map.empty()) {
            static EventSink theSink;
            UI::GetSingleton()->AddEventSink(&theSink);
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SetupLog();
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}