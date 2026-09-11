#include "Events.h"

namespace GlobalRules
{
    namespace
    {
        using EventResult = RE::BSEventNotifyControl;

        RE::PlayerCharacter* Player() { return RE::PlayerCharacter::GetSingleton(); }

        bool IsPlayer(const RE::TESForm* a_form) { return a_form && a_form->IsPlayerRef(); }

        struct ActivateSink : RE::BSTEventSink<RE::TESActivateEvent>
        {
            EventResult ProcessEvent(const RE::TESActivateEvent* a_event, RE::BSTEventSource<RE::TESActivateEvent>*) override
            {
                auto* actionRef = a_event->actionRef.get();
                if (!IsPlayer(actionRef)) {
                    return EventResult::kContinue;
                }
                EventContext ctx;
                ctx.subject = Player();
                ctx.targetRef = a_event->objectActivated.get();
                ctx.targetForm = ctx.targetRef;
                EventManager::Get().Dispatch("activate", ctx);
                return EventResult::kContinue;
            }
        };

        struct EquipSink : RE::BSTEventSink<RE::TESEquipEvent>
        {
            EventResult ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
            {
                auto* actor = a_event->actor.get();
                if (!IsPlayer(actor)) {
                    return EventResult::kContinue;
                }
                EventContext ctx;
                ctx.subject = Player();
                ctx.targetRef = Player();
                ctx.targetForm = RE::TESForm::LookupByID(a_event->baseObject);
                ctx.params["equipped"] = a_event->equipped ? 1.0 : 0.0;
                EventManager::Get().Dispatch("equip", ctx);
                return EventResult::kContinue;
            }
        };

        struct KillSink : RE::BSTEventSink<RE::ActorKill::Event>
        {
            EventResult ProcessEvent(const RE::ActorKill::Event* a_event, RE::BSTEventSource<RE::ActorKill::Event>*) override
            {
                if (!IsPlayer(a_event->killer)) {
                    return EventResult::kContinue;
                }
                EventContext ctx;
                ctx.subject = Player();
                ctx.targetRef = a_event->victim;
                ctx.targetForm = a_event->victim;
                EventManager::Get().Dispatch("kill", ctx);
                return EventResult::kContinue;
            }
        };

        struct MenuSink : RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
            EventResult ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                EventContext ctx;
                ctx.subject = Player();
                ctx.targetRef = Player();
                ctx.targetName = a_event->menuName.c_str();
                ctx.params["opening"] = a_event->opening ? 1.0 : 0.0;
                EventManager::Get().Dispatch("menu", ctx);
                return EventResult::kContinue;
            }
        };

        struct ContainerChangedSink : RE::BSTEventSink<RE::TESContainerChangedEvent>
        {
            EventResult ProcessEvent(const RE::TESContainerChangedEvent* a_event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
            {
                constexpr RE::FormID kPlayerRef = 0x14;
                if (a_event->newContainer != kPlayerRef && a_event->oldContainer != kPlayerRef) {
                    return EventResult::kContinue;
                }
                EventContext ctx;
                ctx.subject = Player();
                ctx.targetRef = Player();
                ctx.targetForm = RE::TESForm::LookupByID(a_event->baseObj);
                ctx.params["count"] = static_cast<double>(a_event->itemCount);
                EventManager::Get().Dispatch("container_changed", ctx);
                return EventResult::kContinue;
            }
        };

        struct QuestStageSink : RE::BSTEventSink<RE::TESQuestStageEvent>
        {
            EventResult ProcessEvent(const RE::TESQuestStageEvent* a_event, RE::BSTEventSource<RE::TESQuestStageEvent>*) override
            {
                EventContext ctx;
                ctx.subject = Player();
                ctx.targetRef = Player();
                ctx.targetForm = RE::TESForm::LookupByID(a_event->formID);
                ctx.params["stage"] = static_cast<double>(a_event->stage);
                EventManager::Get().Dispatch("quest_stage", ctx);
                return EventResult::kContinue;
            }
        };

        struct CellChangeSink : RE::BSTEventSink<RE::BGSActorCellEvent>
        {
            EventResult ProcessEvent(const RE::BGSActorCellEvent* a_event, RE::BSTEventSource<RE::BGSActorCellEvent>*) override
            {
                auto* actor = a_event->actor.get().get();
                if (!IsPlayer(actor)) {
                    return EventResult::kContinue;
                }
                EventContext ctx;
                ctx.subject = Player();
                ctx.targetRef = Player();
                ctx.targetForm = RE::TESForm::LookupByID(a_event->cellID);
                // flags is a raw CellFlag value (0 = enter, 1 = leave), not a bitmask;
                // any(kEnter) was always false because kEnter == 0.
                const bool entering = a_event->flags == RE::BGSActorCellEvent::CellFlag::kEnter;
                ctx.params["entering"] = entering ? 1.0 : 0.0;
                EventManager::Get().Dispatch("cell_change", ctx);
                return EventResult::kContinue;
            }
        };

        struct LevelIncreaseSink : RE::BSTEventSink<RE::LevelIncrease::Event>
        {
            EventResult ProcessEvent(const RE::LevelIncrease::Event* a_event, RE::BSTEventSource<RE::LevelIncrease::Event>*) override
            {
                EventContext ctx;
                ctx.subject = Player();
                ctx.targetRef = Player();
                ctx.params["newLevel"] = static_cast<double>(a_event->newLevel);
                EventManager::Get().Dispatch("level_increase", ctx);
                return EventResult::kContinue;
            }
        };

        ActivateSink          g_activateSink;
        EquipSink             g_equipSink;
        KillSink              g_killSink;
        MenuSink              g_menuSink;
        ContainerChangedSink  g_containerSink;
        QuestStageSink        g_questStageSink;
        CellChangeSink        g_cellSink;
        LevelIncreaseSink     g_levelSink;
    }

    EventManager& EventManager::Get()
    {
        static EventManager instance;
        return instance;
    }

    bool IsKnownEvent(std::string_view a_name)
    {
        return a_name == "activate"sv ||
               a_name == "equip"sv ||
               a_name == "kill"sv ||
               a_name == "menu"sv ||
               a_name == "container_changed"sv ||
               a_name == "quest_stage"sv ||
               a_name == "cell_change"sv ||
               a_name == "level_increase"sv;
    }

    void EventManager::SetHandler(Handler a_handler)
    {
        handler_ = std::move(a_handler);
    }

    void EventManager::RegisterAll()
    {
        if (registered_) {
            return;
        }
        const auto holder = RE::ScriptEventSourceHolder::GetSingleton();

        holder->AddEventSink<RE::TESActivateEvent>(&g_activateSink);
        holder->AddEventSink<RE::TESEquipEvent>(&g_equipSink);
        holder->AddEventSink<RE::TESContainerChangedEvent>(&g_containerSink);
        holder->AddEventSink<RE::TESQuestStageEvent>(&g_questStageSink);

        RE::ActorKill::GetEventSource()->AddEventSink(&g_killSink);
        RE::LevelIncrease::GetEventSource()->AddEventSink(&g_levelSink);
        RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&g_menuSink);
        RE::PlayerCharacter::GetSingleton()->AsBGSActorCellEventSource()->AddEventSink(&g_cellSink);

        registered_ = true;
    }

    void EventManager::UnregisterAll()
    {
        if (!registered_) {
            return;
        }
        const auto holder = RE::ScriptEventSourceHolder::GetSingleton();

        holder->RemoveEventSink<RE::TESActivateEvent>(&g_activateSink);
        holder->RemoveEventSink<RE::TESEquipEvent>(&g_equipSink);
        holder->RemoveEventSink<RE::TESContainerChangedEvent>(&g_containerSink);
        holder->RemoveEventSink<RE::TESQuestStageEvent>(&g_questStageSink);

        RE::ActorKill::GetEventSource()->RemoveEventSink(&g_killSink);
        RE::LevelIncrease::GetEventSource()->RemoveEventSink(&g_levelSink);
        RE::UI::GetSingleton()->RemoveEventSink<RE::MenuOpenCloseEvent>(&g_menuSink);
        RE::PlayerCharacter::GetSingleton()->AsBGSActorCellEventSource()->RemoveEventSink(&g_cellSink);

        registered_ = false;
    }

    void EventManager::Dispatch(std::string_view a_name, const EventContext& a_ctx)
    {
        if (handler_) {
            handler_(a_name, a_ctx);
        }
    }
}
