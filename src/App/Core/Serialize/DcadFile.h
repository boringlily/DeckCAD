#pragma once
#include "Dto.h"

#include <glaze/json.hpp>

#include <string>
#include <string_view>

// The JSON layer: DocumentDto <-> a `.dcad` text buffer, over glaze.
//
// EXCEPTION-FREE. glaze returns an error context, never throws — a malformed or
// truncated file yields `false` plus a human-readable diagnostic rather than a crash or
// an abort. (This is exactly why glaze was chosen over nlohmann, whose no-exception mode
// aborts the process on a parse error.)
//
// This layer is pure text <-> DTO. Files, paths, and the exe directory live in the app
// storage wrapper, so this stays raylib-free and fully testable in core_tests.
namespace Serialize {

struct SerError {
    bool ok { true };
    std::string message;
};

// Serialize a document to pretty-printed JSON. Pretty because a `.dcad` is meant to be
// diffable and hand-inspectable; the size cost is irrelevant for a CAD document.
inline std::string WriteDcad(const DocumentDto& dto)
{
    std::string out;
    // write_json returns an error context; for these plain aggregates it cannot fail, so
    // an empty string is only ever produced by an empty document, never by an error.
    (void)glz::write<glz::opts { .prettify = true }>(dto, out);
    return out;
}

// Parse JSON into a document DTO. Returns false + a diagnostic on malformed input; never
// throws or aborts.
inline bool ReadDcad(std::string_view json, DocumentDto& out, SerError& err)
{
    glz::error_ctx ec = glz::read_json(out, json);
    if (ec) {
        err.ok = false;
        err.message = glz::format_error(ec, json); // located, human-readable
        return false;
    }
    err.ok = true;
    err.message.clear();
    return true;
}

} // namespace Serialize
