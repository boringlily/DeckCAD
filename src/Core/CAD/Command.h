#pragma once
#include "CommandIds.h"
#include <type_traits>

/// @brief A command can have parameters that by-default are not set,
/// this allows the logic to detect if required parameters have been set for the command to be processed by the GeometryEngine
template <typename T>
class CommandParameter {
public:
    explicit CommandParameter() = default;
    explicit CommandParameter(T param)
        : value { param } {};

    bool IsSet()
    {
        return value.has_value();
    }

    void Set(T _value)
    {
        value = _value;
    }

    void Clear()
    {
        value = std::nullopt;
    };

private:
    std::optional<T> value { std::nullopt };
};

class Command {
public:
    explicit Command() = default;

    /// @brief A valid command is one that has all the required parameters set to be processed by the Geometry Engine.
    /// @return True if command is valid.
    virtual bool IsValid() = 0;

protected:
    enum class Flag : u32 {
        Suppressed,

        // Sketch and Wireframe Only
        Construction,
    };

    void SetFlag(Flag flag, bool val)
    {
        if (val) {
            flags |= 1 << static_cast<u32>(flag);
        } else {
            flags &= ~(1 << static_cast<u32>(flag));
        }
    }

    bool GetFlag(Flag flag)
    {
        return flags & (1 << static_cast<u32>(flag));
    }

private:
    template <typename T, typename... AllowedTypes>
    struct is_allowed_type : std::disjunction<std::is_same<T, AllowedTypes>...> {
    };

    template <typename T, typename... AllowedTypes>
    static constexpr bool is_allowed_type_v = is_allowed_type<T, AllowedTypes...>::value;

    u32 flags { 0 };
};

class GeneralCommand : public Command {
public:
    explicit GeneralCommand() = default;
};

class SketchCommand : public Command {
public:
    explicit SketchCommand() = default;
};