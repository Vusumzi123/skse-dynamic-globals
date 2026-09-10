#include "Expression.h"

#include <cstdlib>

namespace GlobalRules
{
    namespace
    {
        // Fixed variable layout. Indices map to values_[].
        constexpr std::size_t kVarCount = 15;

        const char* const kVarNames[kVarCount] = {
            "x", "level", "gold", "health", "magicka", "stamina",
            "carryweight", "speech", "count", "stage", "newLevel",
            "equipped", "opening", "entering", "targetFormID"
        };

        int VarIndex(std::string_view a_name)
        {
            for (std::size_t i = 0; i < kVarCount; ++i) {
                if (a_name == kVarNames[i]) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        // Custom functions (tinyexpr built-ins lack these).
        double fn_min(double a, double b) { return a < b ? a : b; }
        double fn_max(double a, double b) { return a > b ? a : b; }
        double fn_clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
        double fn_round(double v) { return std::floor(v + 0.5); }
        double fn_if(double c, double a, double b) { return c != 0.0 ? a : b; }
        double fn_mod(double a, double b) { return b == 0.0 ? 0.0 : (a - b * std::floor(a / b)); }

        bool ParseConstant(const std::string& a_src, double& a_out)
        {
            char* end = nullptr;
            const double v = std::strtod(a_src.c_str(), &end);
            if (end == a_src.c_str() || end == nullptr) {
                return false;
            }
            // Reject trailing non-whitespace.
            while (*end) {
                if (!std::isspace(static_cast<unsigned char>(*end))) {
                    return false;
                }
                ++end;
            }
            a_out = v;
            return true;
        }
    }

    Expression::~Expression()
    {
        if (expr_) {
            te_free(expr_);
        }
    }

    Expression::Expression(Expression&& a_rhs) noexcept
    {
        *this = std::move(a_rhs);
    }

    Expression& Expression::operator=(Expression&& a_rhs) noexcept
    {
        if (this != &a_rhs) {
            if (expr_) {
                te_free(expr_);
            }
            src_ = std::move(a_rhs.src_);
            expr_ = a_rhs.expr_;
            isConstant_ = a_rhs.isConstant_;
            constantValue_ = a_rhs.constantValue_;
            compiled_ = a_rhs.compiled_;
            std::copy(std::begin(a_rhs.values_), std::end(a_rhs.values_), values_);
            vars_ = std::move(a_rhs.vars_);

            a_rhs.expr_ = nullptr;
            a_rhs.compiled_ = false;
        }
        return *this;
    }

    void Expression::BuildVariables()
    {
        vars_.clear();
        vars_.reserve(kVarCount + 6);

        for (std::size_t i = 0; i < kVarCount; ++i) {
            vars_.push_back({ kVarNames[i], &values_[i], TE_VARIABLE, nullptr });
        }

        vars_.push_back({ "min", (const void*)&fn_min, TE_FUNCTION2, nullptr });
        vars_.push_back({ "max", (const void*)&fn_max, TE_FUNCTION2, nullptr });
        vars_.push_back({ "clamp", (const void*)&fn_clamp, TE_FUNCTION3, nullptr });
        vars_.push_back({ "round", (const void*)&fn_round, TE_FUNCTION1, nullptr });
        vars_.push_back({ "if", (const void*)&fn_if, TE_FUNCTION3, nullptr });
        vars_.push_back({ "mod", (const void*)&fn_mod, TE_FUNCTION2, nullptr });
    }

    bool Expression::Compile(const std::string& a_src)
    {
        src_ = a_src;
        isConstant_ = false;
        compiled_ = false;
        if (expr_) {
            te_free(expr_);
            expr_ = nullptr;
        }

        if (ParseConstant(src_, constantValue_)) {
            isConstant_ = true;
            compiled_ = true;
            return true;
        }

        BuildVariables();
        int err = 0;
        expr_ = te_compile(src_.c_str(), vars_.data(), static_cast<int>(vars_.size()), &err);
        if (!expr_) {
            if (err > 0) {
                SKSE::log::error("expression '{}' failed to compile at position {}", src_, err);
            } else {
                SKSE::log::error("expression '{}' failed to compile (unknown variable/function)", src_);
            }
            return false;
        }

        compiled_ = true;
        return true;
    }

    void Expression::UpdateValues(double a_x, const std::unordered_map<std::string, double>& a_params)
    {
        for (auto& v : values_) {
            v = 0.0;
        }

        values_[0] = a_x;

        const auto player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            values_[1] = static_cast<double>(player->GetLevel());
            values_[2] = static_cast<double>(player->GetGoldAmount());
            // Must go through AsActorValueOwner(): in multi-runtime builds the
            // ActorValueOwner base is at a conflict placeholder offset, so a direct
            // player->GetActorValue() dispatches through the wrong vtable and crashes.
            const auto* avo = player->AsActorValueOwner();
            values_[3] = static_cast<double>(avo->GetActorValue(RE::ActorValue::kHealth));
            values_[4] = static_cast<double>(avo->GetActorValue(RE::ActorValue::kMagicka));
            values_[5] = static_cast<double>(avo->GetActorValue(RE::ActorValue::kStamina));
            values_[6] = static_cast<double>(avo->GetActorValue(RE::ActorValue::kCarryWeight));
            values_[7] = static_cast<double>(avo->GetActorValue(RE::ActorValue::kSpeech));
        }

        for (const auto& [name, value] : a_params) {
            const int idx = VarIndex(name);
            if (idx >= 0) {
                values_[idx] = value;
            }
        }
    }

    bool Expression::Evaluate(double a_x, const std::unordered_map<std::string, double>& a_params, double& a_out)
    {
        if (!compiled_) {
            return false;
        }
        if (isConstant_) {
            a_out = constantValue_;
            return true;
        }

        UpdateValues(a_x, a_params);
        const double result = te_eval(expr_);
        if (!std::isfinite(result)) {
            return false;
        }
        a_out = result;
        return true;
    }
}
