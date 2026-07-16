#pragma once
#include "DTL.h"
#include "ParameterEngine.h"
#include "SketchDocument.h"
#include <cstring>
#include <string_view>
#include <vector>

// Per-scene Explorer state.
//
// This lives on Scene rather than in function-local statics for two reasons: an App.dll
// hot-reload would wipe a static mid-session, and each scene needs its own tree
// expansion and its own edit buffers.
//
// Ui::InputLabel edits a CALLER-OWNED char buffer, so the ParameterTable needs one
// buffer per row that survives the immediate-mode rebuild. Rows are keyed by UPID (not
// by index) so removing a parameter can't silently re-point a row's buffer at a
// different parameter's data.

inline constexpr u32 kParamNameCap = 64;
inline constexpr u32 kParamExprCap = 256;

struct ParamRow {
    Param::UPID id { Param::kNullUpid };
    char name[kParamNameCap] {};
    u32 nameLen { 0 };
    char expr[kParamExprCap] {};
    u32 exprLen { 0 };
};

inline void SetBuf(char* buf, u32& len, u32 cap, std::string_view src)
{
    u32 n = static_cast<u32>(src.size());
    if (n > cap - 1) {
        n = cap - 1;
    }
    std::memcpy(buf, src.data(), n);
    buf[n] = '\0';
    len = n;
}

// The Explorer's tabs. Each is its own table over a different view of the model:
//   Model      the command list — what the user authored
//   Parameters the ParametricExpressions — what drives it
//   Geometry   the generated entities — what came out
// Distinct enough that stacking them would just make each one cramped.
enum class ExplorerTab : u8 {
    Model,
    Parameters,
    Geometry,
};

struct ExplorerTabInfo {
    ExplorerTab tab;
    std::string_view name;
};

inline constexpr ExplorerTabInfo kExplorerTabs[] = {
    { ExplorerTab::Model, "Model" },
    { ExplorerTab::Parameters, "Parameters" },
    { ExplorerTab::Geometry, "Geometry" },
};

struct ExplorerState {
    ExplorerTab tab { ExplorerTab::Model };

    // ── command tree ─────────────────────────────────────────────────────────
    // Collapsed rather than expanded, so a newly created group starts open — which is
    // what you want right after making one.
    std::vector<FeatureId> collapsed;
    FeatureId selected { kNullFeature };

    bool IsCollapsed(FeatureId id) const
    {
        for (FeatureId c : collapsed) {
            if (c == id) {
                return true;
            }
        }
        return false;
    }

    void ToggleCollapsed(FeatureId id)
    {
        for (u32 k = 0; k < collapsed.size(); ++k) {
            if (collapsed[k] == id) {
                collapsed.erase(collapsed.begin() + k);
                return;
            }
        }
        collapsed.push_back(id);
    }

    // ── parameter table ──────────────────────────────────────────────────────
    std::vector<ParamRow> rows;

    // The "add parameter" row.
    char newName[kParamNameCap] {};
    u32 newNameLen { 0 };
    char newExpr[kParamExprCap] {};
    u32 newExprLen { 0 };

    ParamRow* FindRow(Param::UPID id)
    {
        for (ParamRow& r : rows) {
            if (r.id == id) {
                return &r;
            }
        }
        return nullptr;
    }

    // Reconcile rows against the engine: add rows for new parameters, drop rows for
    // removed ones.
    //
    // Existing rows are deliberately NOT refreshed from the engine. Once a row exists
    // its buffer is the user's, and re-filling it every frame would fight whatever they
    // are typing — the row pushes to the engine, never the other way round.
    void SyncRows(const Param::ParameterEngine& engine)
    {
        // Drop rows whose parameter is gone.
        for (u32 k = 0; k < rows.size();) {
            if (engine.Get(rows[k].id) == nullptr) {
                rows.erase(rows.begin() + k);
            } else {
                ++k;
            }
        }

        // Add rows for parameters that don't have one yet.
        for (const Param::ParametricExpression& p : engine.Parameters()) {
            if (FindRow(p.Id()) != nullptr) {
                continue;
            }
            ParamRow r {};
            r.id = p.Id();
            SetBuf(r.name, r.nameLen, kParamNameCap, p.Name());
            SetBuf(r.expr, r.exprLen, kParamExprCap, p.Text());
            rows.push_back(r);
        }
    }

    void ClearNewRow()
    {
        newName[0] = '\0';
        newNameLen = 0;
        newExpr[0] = '\0';
        newExprLen = 0;
    }
};
