#include "Engine.h"

#include "Condition.h"
#include "FormId.h"
#include "Persistence.h"

namespace GlobalRules
{
    Engine& Engine::Get()
    {
        static Engine instance;
        return instance;
    }

    bool Engine::DebugEnabled() const
    {
        if (debugGlobalForm_ && debugGlobalForm_->value != 0.0f) {
            return true;
        }
        return config_.debug;
    }

    void Engine::ApplyLogLevel()
    {
        spdlog::level::level_enum level = spdlog::level::info;
        if (config_.logLevel == "trace") {
            level = spdlog::level::trace;
        } else if (config_.logLevel == "debug") {
            level = spdlog::level::debug;
        } else if (config_.logLevel == "info") {
            level = spdlog::level::info;
        } else if (config_.logLevel == "warn" || config_.logLevel == "warning") {
            level = spdlog::level::warn;
        } else if (config_.logLevel == "error" || config_.logLevel == "err") {
            level = spdlog::level::err;
        } else if (config_.logLevel == "critical") {
            level = spdlog::level::critical;
        }

        if (config_.debug && level > spdlog::level::debug) {
            level = spdlog::level::debug;
        }

        auto logger = spdlog::default_logger();
        if (logger) {
            logger->set_level(level);
            logger->flush_on(level);
        }
    }

    void Engine::Load()
    {
        config_ = Config::Load();
        ApplyLogLevel();

        debugGlobalForm_ = config_.debugGlobal.empty() ? nullptr
            : ResolveFormAs<RE::TESGlobal>(config_.debugGlobal);

        rules_.clear();
        byEvent_.clear();

        if (!config_.enabled) {
            SKSE::log::info("GlobalRules: disabled by config; no rules loaded");
            return;
        }

        rules_ = LoadRules(config_.rulesDirectory);

        for (std::size_t i = 0; i < rules_.size(); ++i) {
            byEvent_[rules_[i].event].push_back(i);
        }

        SKSE::log::info("GlobalRules: {} rule(s) indexed across {} event type(s)", rules_.size(), byEvent_.size());
    }

    void Engine::OnDataLoaded()
    {
        Load();

        if (config_.enabled) {
            EventManager::Get().SetHandler([](std::string_view a_name, const EventContext& a_ctx) {
                Engine::Get().OnEvent(a_name, a_ctx);
            });
            EventManager::Get().RegisterAll();
        }

        SKSE::log::info("GlobalRules: initialized");
    }

    void Engine::Reload()
    {
        Load();
        SKSE::log::info("GlobalRules: rules reloaded");
    }

    bool Engine::TargetMatches(const Rule& a_rule, std::string_view a_event, const EventContext& a_ctx) const
    {
        if (a_event == "menu") {
            if (a_rule.targetName.empty()) {
                return true;  // wildcard
            }
            return a_ctx.targetName == a_rule.targetName;
        }

        if (!a_rule.hasTarget) {
            return true;  // wildcard
        }
        if (a_rule.targetFormID != 0) {
            return a_ctx.targetForm != nullptr && a_ctx.targetForm->GetFormID() == a_rule.targetFormID;
        }
        return a_rule.target == a_ctx.targetForm;
    }

    void Engine::OnEvent(std::string_view a_name, const EventContext& a_ctx)
    {
        auto it = byEvent_.find(std::string(a_name));
        if (it == byEvent_.end()) {
            return;
        }

        if (a_ctx.subject && !a_ctx.subject->IsPlayerRef()) {
            return;
        }

        const bool verbose = DebugEnabled();
        RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

        for (const auto ruleIdx : it->second) {
            const Rule& rule = rules_[ruleIdx];

            if (!TargetMatches(rule, a_name, a_ctx)) {
                continue;
            }

            RE::TESObjectREFR* targetRef = a_ctx.targetRef ? a_ctx.targetRef : player;
            const bool passed = CheckPerk(rule.perk, targetRef, rule.invert);

            if (verbose) {
                const std::string targetDesc = (a_name == "menu")
                    ? (a_ctx.targetName.empty() ? std::string("*") : a_ctx.targetName)
                    : FormName(a_ctx.targetForm);
                SKSE::log::debug(
                    "event={} target={} rule=#{} perk={} {}",
                    a_name,
                    targetDesc,
                    rule.index,
                    rule.perk ? FormName(rule.perk) : std::string("<none>"),
                    passed ? "PASS" : "FAIL");
            }

            if (!passed) {
                continue;
            }

            auto params = a_ctx.params;
            params["targetFormID"] = a_ctx.targetForm ? static_cast<double>(a_ctx.targetForm->GetFormID()) : 0.0;

            double newVal = 0.0;
            const double x = static_cast<double>(rule.global->value);
            if (!rule.expr->Evaluate(x, params, newVal)) {
                SKSE::log::warn(
                    "rule #{} global '{}' expression '{}' produced a non-finite value; write skipped",
                    rule.index, FormName(rule.global), rule.expr->Source());
                continue;
            }

            const float oldValue = rule.global->value;

            if (config_.dryRun) {
                SKSE::log::info(
                    "  [dry-run] {} [0x{:08X}]: {} -> {}  (expr \"{}\")",
                    FormName(rule.global),
                    rule.global->GetFormID(),
                    oldValue,
                    newVal,
                    rule.expr->Source());
                continue;
            }

            rule.global->value = static_cast<float>(newVal);
            Persistence::AppliedValues()[rule.global->GetFormID()] = static_cast<float>(newVal);

            if (config_.logChanges) {
                SKSE::log::info(
                    "  {} [0x{:08X}]: {} -> {}  (expr \"{}\")",
                    FormName(rule.global),
                    rule.global->GetFormID(),
                    oldValue,
                    rule.global->value,
                    rule.expr->Source());
            } else if (verbose) {
                SKSE::log::debug(
                    "  {} [0x{:08X}]: {} -> {}  (expr \"{}\")",
                    FormName(rule.global),
                    rule.global->GetFormID(),
                    oldValue,
                    rule.global->value,
                    rule.expr->Source());
            }
        }
    }
}
