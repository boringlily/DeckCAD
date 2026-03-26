#pragma once
#include "DumbTypes.h"
#include <algorithm>
#include <optional>
#include <string>
#include <map>

// Unique Parameter IDentifier
using UPID = u32;

enum class ParameterUnit : u32 {
    Number,
    Length,
    Angle
};

/// @brief An error object used
class ParserError {
public:
    enum {
        None,
        Invalid,
        UnknownParameter,
        UnknownFunction,
        ParanthesisMismatch
    } type;

    std::string_view msg;

private:
    ParserError() = delete;
    ParserError(std::string msg)
        : msg { msg } {};
};

// A user parameter is a referenceable expression, provides the ability to have more complex denpendency based behavior.
class ParametricExpression {
public:
    ParametricExpression() = delete;
    ParametricExpression(u32 uid, std::string expression = "", std::string name = "")
        : uid { uid }
        , expression { expression }
        , name { name } {};

    static bool Compare(ParametricExpression& a, ParametricExpression& b)
    {
        return a.uid < b.uid;
    }

private:
    const UPID uid; // Unique ID
    std::string name; // Unique name
    std::string expression; // The expression that needs solving.
    ParameterUnit unit { ParameterUnit::Number }; // The value type of the expression resolution.
    std::vector<UPID> dependencies; // Other parameters that the expression depends on.

    /// @brief Tokenize and parse the expression to verify if it is valid.
    bool CheckExpression() { return true; };
};

/// Handles the consturtion, storage, and solution evaluation of all parameteric expressions.
/// Anything that consumes a parameter can only utilize the UPID to retrieve and update parameters.
class ParameterEngine {
public:
    using ParameterPtr = std::unique_ptr<ParametricExpression>;

    UPID CreateNewParameter(std::string expression, std::string name = "")
    {
        UPID id = parameters.size();
        parameters.emplace_back(id, expression, name);
        return id;
    };

    ParametricExpression getParameter(UPID id) { return parameters.at(id); };
    bool updateParameter(ParametricExpression parameter);

private:
    std::vector<ParametricExpression> parameters;
};