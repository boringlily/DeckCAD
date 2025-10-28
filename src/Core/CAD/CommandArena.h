#pragma once
#include <cstddef>
#include <new>
#include <memory>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <typeindex>
#include <cstring>

// --- Context and Type Definitions ---

enum GeometryCommandType {
    Command_Invalid = 0,
    // Global Commands
    Command_DrawLine,
    Command_DrawCircle,
    Command_CreateSketch, // New command that establishes a context
    // Sketch Context Commands (Lower-Context)
    Command_SketchLine,
    Command_SketchArc,
    Command_SketchCircle,
    Command_Count
};

enum CommandContextType {
    Context_Global,
    Context_Sketch,
    Context_Count
};

// Base Command Structure
struct ICommand {
    GeometryCommandType type;
    CommandContextType context; // New field to identify its context
    ~ICommand() = default;
};

// --- Sub-Arena for Sketch Context ---

// This simple Arena is used internally by CreateSketchCommand to store its sub-commands.
class SubCommandArena {
private:
    std::vector<char> m_buffer; // Use vector for dynamic buffer management
    size_t m_offset = 0;

public:
    SubCommandArena(size_t initial_capacity)
        : m_buffer(initial_capacity)
        , m_offset(0)
    {
    }

    // Allocates and constructs an object onto the sub-arena
    template <typename T, typename... Args>
    T* New(Args&&... args)
    {
        const size_t size = sizeof(T);
        const size_t align = alignof(T);

        // Ensure capacity
        if (m_offset + size > m_buffer.size()) {
            // Simple resize logic: double the size
            m_buffer.resize(m_buffer.size() * 2 + size);
        }

        void* current_ptr = m_buffer.data() + m_offset;
        size_t space = m_buffer.size() - m_offset;

        void* aligned_ptr = std::align(align, size, current_ptr, space);
        assert(aligned_ptr != nullptr && "SubCommandArena: Alignment error or unexpected overflow!");

        m_offset = static_cast<char*>(aligned_ptr) - m_buffer.data() + size;

        T* object_ptr = new (aligned_ptr) T(std::forward<Args>(args)...);
        return std::launder(object_ptr);
    }

    // Simplistic clear for the sub-arena
    void Clear()
    {
        // NOTE: In a full system, you would call destructors here.
        // For simplicity, we assume Sketch Commands are trivially destructible or handled elsewhere.
        m_offset = 0;
    }

    // Get the raw data size (used for final allocation size)
    size_t GetSize() const { return m_offset; }
    const char* GetData() const { return m_buffer.data(); }

    // Copy the contents of another SubCommandArena (used in UpdateObject)
    void CopyFrom(const SubCommandArena& other)
    {
        m_buffer.resize(other.m_buffer.size());
        std::memcpy(m_buffer.data(), other.m_buffer.data(), other.m_offset);
        m_offset = other.m_offset;
    }
};

// --- Command Subclasses ---

// A simple sketch command stored in the sub-arena
struct SketchLineCommand : public ICommand {
    float x_start, y_start;
    SketchLineCommand(float xs, float ys)
        : x_start(xs)
        , y_start(ys)
    {
        type = Command_SketchLine;
        context = Context_Sketch;
    }
};

// The command that establishes the Sketch context
struct CreateSketchCommand : public ICommand {
    size_t data_size;
    // Ptr to the command data in the main arena (will be right after this object)
    // NOTE: This pointer is only valid AFTER FinishCommand() is called.
    char* command_data_ptr = nullptr;

    // Sub-Arena is where WIP Sketch commands are temporarily stored
    SubCommandArena sub_arena;

    CreateSketchCommand()
        : sub_arena(512)
    {
        type = Command_CreateSketch;
        context = Context_Global;
    }
    // No explicit destructor needed here, as the sub-arena is RAII.
};

// --- 3. Command Manager (Main Logic) ---

class CommandManager {
private:
    CommandArena m_global_arena;
    ICommand* m_active_command = nullptr;
    std::vector<CommandContextType> m_context_stack = { Context_Global }; // Initial context

    // Helper to get the actual type from a command instance
    GeometryCommandType get_type(const ICommand* cmd)
    {
        if (auto sketch_cmd = dynamic_cast<CreateSketchCommand*>(cmd)) {
            return Command_CreateSketch;
        }
        return cmd->type;
    }

    // Helper to determine if a command is specific to the current active context
    bool is_valid_in_current_context(GeometryCommandType type)
    {
        CommandContextType active_context = m_context_stack.back();

        if (active_context == Context_Global) {
            return type == Command_DrawLine || type == Command_DrawCircle || type == Command_CreateSketch;
        } else if (active_context == Context_Sketch) {
            return type == Command_SketchLine || type == Command_SketchArc || type == Command_SketchCircle;
        }
        return false;
    }

public:
    CommandManager(size_t arena_capacity)
        : m_global_arena(arena_capacity)
    {
    }

    CommandContextType GetActiveContext() const { return m_context_stack.back(); }

    // --- Transactional Command Flow ---

    // 1. StartCommand: Allocates a temporary command object dynamically.
    template <typename T, typename... Args>
    void StartCommand(Args&&... args)
    {
        // Prevent starting a new command if one is already active (to simplify the stack)
        assert(m_active_command == nullptr && "Cannot StartCommand: A command is already active. Must Finish or Cancel first.");

        T* temp_cmd = new T(std::forward<Args>(args)...); // Dynamically allocated

        // Validate command type for the current context
        assert(is_valid_in_current_context(temp_cmd->type) && "StartCommand: Command type is invalid for the current active context.");

        m_active_command = temp_cmd;
        std::cout << "-> STARTED Command " << temp_cmd->type << " (Context: " << GetActiveContext() << ")\n";

        // If the started command is a context switch, push the new context.
        if (temp_cmd->type == Command_CreateSketch) {
            m_context_stack.push_back(Context_Sketch);
            std::cout << "-> CONTEXT PUSHED: Switched to Context_Sketch\n";
        }
    }

    // 2. UpdateCommand: Updates the active command (either the main one or the sub-arena one).
    template <typename T, typename... Args>
    void UpdateCommand(Args&&... args)
    {
        assert(m_active_command != nullptr && "Cannot UpdateCommand: No command is currently active.");

        // If the active command is a Sketch context command, place it in the sub-arena.
        if (GetActiveContext() == Context_Sketch) {
            // Must cast to the current command type (CreateSketchCommand) to access its sub-arena.
            auto sketch_cmd = static_cast<CreateSketchCommand*>(m_active_command);

            // Allocate the new sub-command onto the sub-arena.
            T* sub_cmd = sketch_cmd->sub_arena.New<T>(std::forward<Args>(args)...);
            std::cout << "-> UPDATE (SUB-ARENA): Constructed sub-command " << sub_cmd->type << "\n";

        } else {
            // Global context: Construct a new temp object and replace the old one.
            // This allows the temp command to "grow" in size during its construction phase.
            GeometryCommandType old_type = get_type(m_active_command);
            assert(std::is_same_v<T, std::decay_t<decltype(*m_active_command)>>() && "UpdateCommand: Cannot change type of active command.");

            ICommand* new_cmd = new T(std::forward<Args>(args)...);
            delete m_active_command; // Delete the old version
            m_active_command = new_cmd;
            std::cout << "-> UPDATE (GLOBAL): Replaced temp command " << old_type << "\n";
        }
    }

    // 3. FinishCommand: Commits the active command to the global arena.
    void FinishCommand()
    {
        assert(m_active_command != nullptr && "Cannot FinishCommand: No command is currently active.");

        // If in a lower context (e.g., Sketch), this finish only applies to the sub-context.
        if (GetActiveContext() == Context_Sketch) {
            // NOTE: The current requirement implies that "FinishCommand" only finalizes the sub-command,
            // but the overall CreateSketchCommand remains active until a second Finish/Cancel.
            // We assume a dedicated sub-command "FINISH" or similar for this.
            // For now, we will assume this Finish finalizes the overall command if there are no sub-contexts.
            std::cout << "-> FINISH (SUB-CONTEXT): Assuming sub-commands are handled. Command remains active.\n";
            return;
        }

        // --- Global Command Finalization ---
        GeometryCommandType type = m_active_command->type;

        if (type == Command_CreateSketch) {
            auto temp_sketch_cmd = static_cast<CreateSketchCommand*>(m_active_command);
            size_t final_data_size = temp_sketch_cmd->sub_arena.GetSize();
            size_t final_cmd_size = sizeof(CreateSketchCommand) + final_data_size;

            // 1. Allocate block on the main arena
            CreateSketchCommand* final_cmd = m_global_arena.New<CreateSketchCommand>(final_cmd_size);

            // 2. Copy the active command's state (including the sub-arena's data)
            // Use placement new to reconstruct the object at the target location
            new (final_cmd) CreateSketchCommand(*temp_sketch_cmd);

            // 3. Copy the sub-arena data immediately after the command object
            final_cmd->command_data_ptr = reinterpret_cast<char*>(final_cmd) + sizeof(CreateSketchCommand);
            final_cmd->data_size = final_data_size;
            std::memcpy(final_cmd->command_data_ptr, temp_sketch_cmd->sub_arena.GetData(), final_data_size);

            // 4. Pop Context and Cleanup
            m_context_stack.pop_back();
            std::cout << "-> CONTEXT POPPED: Switched back to Global\n";
            delete m_active_command;
            m_active_command = nullptr;

            std::cout << "-> FINISHED Command " << type << " onto arena (Size: " << final_cmd_size << ")\n";
        } else {
            // For simple commands, allocate directly onto the arena and clean up.
            m_global_arena.NewCopy(m_active_command);
            delete m_active_command;
            m_active_command = nullptr;
            std::cout << "-> FINISHED Command " << type << " onto arena\n";
        }
    }

    // 4. CancelCommand: Deletes the temporary command object.
    void CancelCommand()
    {
        assert(m_active_command != nullptr && "Cannot CancelCommand: No command is currently active.");

        // If in a lower context, this cancel only applies to the sub-context.
        if (GetActiveContext() == Context_Sketch) {
            auto sketch_cmd = static_cast<CreateSketchCommand*>(m_active_command);
            sketch_cmd->sub_arena.Clear();
            std::cout << "-> CANCELED (SUB-CONTEXT): Cleared Sketch sub-commands. Command remains active.\n";
            return;
        }

        // Global Command Cancellation
        GeometryCommandType type = m_active_command->type;

        // Pop context if the command started one
        if (type == Command_CreateSketch) {
            m_context_stack.pop_back();
            std::cout << "-> CONTEXT POPPED: Switched back to Global\n";
        }

        delete m_active_command;
        m_active_command = nullptr;
        std::cout << "-> CANCELED Command " << type << "\n";
    }
};

// --- CommandArena Helper (Simplified for this transactional flow) ---

// The global arena only needs basic allocation and a copy function.
class CommandArena {
private:
    char* m_buffer;
    size_t m_capacity;
    size_t m_offset = 0;
    // (Destructor list omitted for brevity, assume trivial Global commands for this example)

public:
    CommandArena(size_t capacity)
        : m_capacity(capacity)
    {
        m_buffer = new char[m_capacity];
    }
    ~CommandArena() { delete[] m_buffer; }

    // New: Allocates a block of memory and returns a pointer
    template <typename T>
    T* New(size_t required_size)
    {
        // Simple allocation without alignment for this simplified example
        size_t size = required_size;

        void* aligned_ptr = m_buffer + m_offset; // Skip alignment for simplicity

        if (m_offset + size > m_capacity) {
            assert(false && "Global Arena Out of Space!");
            return nullptr;
        }

        m_offset += size;
        return reinterpret_cast<T*>(aligned_ptr);
    }

    // NewCopy: Allocates exact size and copies data from a temporary object
    void NewCopy(ICommand* temp_cmd)
    {
        size_t size = sizeof(*temp_cmd); // Simpification: assumes basic commands have same size

        void* dest = m_buffer + m_offset;
        if (m_offset + size > m_capacity) {
            assert(false && "Global Arena Out of Space!");
            return;
        }

        // Copy data and update offset
        std::memcpy(dest, temp_cmd, size);
        m_offset += size;
    }

    // Simplistic Clear
    void ClearAll() { m_offset = 0; }
};

// --- Example Usage ---

int main()
{
    CommandManager manager(1024 * 4); // 4KB global arena

    std::cout << "--- 1. Simple Command Transaction (DrawLine) ---\n";
    manager.StartCommand<DrawLineCommand>(1.0f, 2.0f, 3.0f, 4.0f);
    manager.UpdateCommand<DrawLineCommand>(1.0f, 2.0f, 5.0f, 6.0f); // New version
    manager.FinishCommand();

    std::cout << "\n--- 2. Nested Command Transaction (CreateSketch) ---\n";
    // 2a. Start top-level command
    manager.StartCommand<CreateSketchCommand>();

    // Check context
    std::cout << "Current Context: " << manager.GetActiveContext() << "\n";

    // 2b. Start adding sub-commands (UpdateCommand is used for sub-commands)
    manager.UpdateCommand<SketchLineCommand>(10.0f, 20.0f);
    manager.UpdateCommand<SketchLineCommand>(30.0f, 40.0f);

    // 2c. Cancel the sub-commands (only clears the sub-arena)
    manager.CancelCommand();

    // 2d. Re-add sub-commands
    manager.UpdateCommand<SketchCircleCommand>(5);
    manager.UpdateCommand<SketchLineCommand>(60.0f, 70.0f);

    // 2e. Finalize the top-level command
    manager.FinishCommand();

    // Check context (should be back to Global)
    std::cout << "Current Context: " << manager.GetActiveContext() << "\n";

    return 0;
}