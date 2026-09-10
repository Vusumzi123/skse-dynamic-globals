#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct te_expr;
struct te_variable;

namespace GlobalRules
{
    class Expression
    {
    public:
        Expression() = default;
        ~Expression();

        Expression(const Expression&) = delete;
        Expression& operator=(const Expression&) = delete;

        Expression(Expression&&) noexcept;
        Expression& operator=(Expression&&) noexcept;

        // Compiles the expression source. Returns true on success.
        bool Compile(const std::string& a_src);

        // Evaluates the expression using the given current global value `a_x` and
        // event params. Player stats are read live. Returns false if not compiled
        // or the result is non-finite.
        bool Evaluate(double a_x, const std::unordered_map<std::string, double>& a_params, double& a_out);

        [[nodiscard]] const std::string& Source() const { return src_; }
        [[nodiscard]] bool IsConstant() const { return isConstant_; }
        [[nodiscard]] double ConstantValue() const { return constantValue_; }

    private:
        void BuildVariables();
        void UpdateValues(double a_x, const std::unordered_map<std::string, double>& a_params);

        std::string src_;
        te_expr* expr_ = nullptr;
        bool isConstant_ = false;
        double constantValue_ = 0.0;
        bool compiled_ = false;

        // Persistent storage bound to te_variable addresses.
        double values_[15]{};
        std::vector<te_variable> vars_;
    };
}
