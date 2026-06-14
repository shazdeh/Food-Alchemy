#include "logger.h"
#include "CLibUtilsQTR/FormReader.hpp"

std::unordered_map<TESBoundObject*, TESBoundObject*> map;

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

bool IsAlchemyBench(TESObjectREFR* ref) {
    if (!ref) return false;
    if (auto* base = ref->GetBaseObject(); base) {
        if (base->Is(FormType::Furniture)) {
            TESFurniture* furniture = base->As<TESFurniture>();
            if (furniture && furniture->workBenchData.benchType == TESFurniture::WorkBenchData::BenchType::kAlchemy) {
                return true;
            }
        }
    }
    return false;
}

struct EventSink : public BSTEventSink<TESFurnitureEvent> {
    BSEventNotifyControl ProcessEvent(const TESFurnitureEvent* event, BSTEventSource<TESFurnitureEvent>*) {
        if (!event || event->actor.get() != PlayerCharacter::GetSingleton() || !IsAlchemyBench(event->targetFurniture.get())) return BSEventNotifyControl::kContinue;

        if (event->type == TESFurnitureEvent::FurnitureEventType::kEnter) {
            ConvertFoodToIngredient();
        } else {
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
            ScriptEventSourceHolder().GetSingleton()->AddEventSink<TESFurnitureEvent>(&theSink);
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SetupLog();
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}