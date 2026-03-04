#pragma once
#include <type_traits>

template <typename CommandType>
requires std::is_enum_v<CommandType>
class Command {
public:
    explicit Command() {};
    explicit Command(CommandType type)
        : type { type } {};

    /// @brief A valid command is one that has all the required parameters set to be processed by the Geometry Engine.
    /// @return True if command is valid.
    virtual bool IsValid() const
    {
        return true;
    };

    bool IsFinished() const
    {
        return GetFlag(Flag::Finished);
    }

    CommandType type;
    u32 id;

protected:
    enum class Flag : u32 {
        Finished,
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