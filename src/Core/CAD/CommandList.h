#pragma once
#include "raylib.h"
#include <vector>
#include <variant>
#include <string>

// using GeometryId = u32;

// enum class GeometryCommandType : u32
// {
//     GeneralContextCommandsStart = 0,
//     AddSketch,
// };

// /// @brief Commands that can be issued into a sketch context
// enum class SketchCommandType : u32
// {
//     Line
// };

// // enum class GeometryArgumentType : u32
// // {

// // };

// using CommandArgument = std::variant<float, Vector2, Vector3, Vector4, std::string, GeometryId>;

// template<typename CommandType>
// struct Command
// {
//     CommandType command;
//     std::vector<CommandArgument> arguments;
// };

// using GeometryCommand = Command<GeometryCommandType>;
// using SketchCommand = Command<SketchCommandType>;

// using SketchCommandList = std::vector<SketchCommand>;

// class CommandList
// {
//    public:
//    std::vector<SketchCommandList> sketches{};
//    std::vector<GeometryCommand> geometry_commands{};
// };

// class ICommand
// {
//     public:
//     GeometryCommandType type;
// }