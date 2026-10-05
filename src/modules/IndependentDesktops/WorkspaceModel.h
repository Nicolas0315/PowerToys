// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#pragma once

#include "Constants.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace IndependentDesktops
{
    struct WindowId
    {
        std::uint64_t handle{};
        std::uint32_t process{};
        std::uint32_t thread{};
        std::uint64_t created{};
        std::uint64_t incarnation{};
        auto key() const { return std::tie(handle, process, thread, created, incarnation); }
        bool operator<(const WindowId& other) const { return key() < other.key(); }
        bool operator==(const WindowId& other) const { return key() == other.key(); }
    };
    struct WindowSnapshot
    {
        WindowId id;
        WindowId root;
        std::wstring monitor;
        std::wstring desktop;
        bool visible{};
        bool manageable{};
    };
    enum class Rejection
    {
        None,
        NoMonitor,
        CrossMonitorGroup,
        Capacity,
        Stale
    };
    struct Transition
    {
        Rejection rejection{ Rejection::NoMonitor };
        std::uint64_t version{};
        std::wstring desktop;
        std::wstring monitor;
        std::optional<unsigned> selection;
        std::vector<WindowId> hide;
        std::vector<WindowId> show;
        std::map<WindowId, unsigned> assignments;
        bool valid() const { return rejection == Rejection::None; }
    };
    class WorkspaceModel
    {
        struct Entry
        {
            WindowSnapshot window;
            std::wstring assignedMonitor;
            unsigned workspace{};
            bool hiddenByUs{};
        };
        std::map<WindowId, Entry> m_windows;
        std::map<std::pair<std::wstring, std::wstring>, unsigned> m_selected;
        std::uint64_t m_version{};
        bool m_saturated{};

        Transition Plan(const std::wstring& desktop, const std::wstring& monitor, std::optional<unsigned> selection, const std::map<WindowId, unsigned>& assignments = {}) const
        {
            Transition result;
            result.version = m_version;
            result.desktop = desktop;
            result.monitor = monitor;
            result.selection = selection;
            if (desktop.empty() || monitor.empty() || (selection && *selection >= WorkspaceCount))
                return result;
            if (m_saturated)
            {
                result.rejection = Rejection::Capacity;
                return result;
            }
            const unsigned target = selection.value_or(Selected(desktop, monitor));
            result.rejection = Rejection::None;
            result.assignments = assignments;
            std::set<WindowId> affectedGroups;
            for (const auto& [id, entry] : m_windows)
            {
                if (entry.window.desktop != desktop || entry.assignedMonitor != monitor)
                    continue;
                const auto assigned = assignments.find(id);
                const unsigned workspace = assigned == assignments.end() ? entry.workspace : assigned->second;
                if ((selection && *selection != Selected(desktop, monitor)) || assigned != assignments.end() ||
                    (entry.window.visible && workspace != target) || (entry.hiddenByUs && workspace == target))
                    affectedGroups.insert(entry.window.root);
            }
            for (const auto& [id, entry] : m_windows)
            {
                if (entry.window.desktop == desktop && affectedGroups.contains(entry.window.root) && entry.window.monitor != monitor)
                {
                    result.rejection = Rejection::CrossMonitorGroup;
                    result.assignments.clear();
                    return result;
                }
            }
            for (const auto& [id, entry] : m_windows)
            {
                if (entry.window.desktop != desktop || entry.assignedMonitor != monitor)
                    continue;
                const auto assigned = assignments.find(id);
                const unsigned workspace = assigned == assignments.end() ? entry.workspace : assigned->second;
                const bool hide = entry.window.visible && workspace != target;
                const bool show = entry.hiddenByUs && workspace == target;
                if ((hide || show) && entry.window.monitor != monitor)
                {
                    result.rejection = Rejection::CrossMonitorGroup;
                    result.hide.clear();
                    result.show.clear();
                    result.assignments.clear();
                    return result;
                }
                if (hide)
                    result.hide.push_back(id);
                if (show)
                    result.show.push_back(id);
            }
            return result;
        }

    public:
        void Observe(const std::wstring& desktop, const std::vector<WindowSnapshot>& snapshots)
        {
            ++m_version;
            std::map<WindowId, WindowSnapshot> observed;
            for (const auto& window : snapshots)
                observed.emplace(window.id, window);
            for (auto it = m_windows.begin(); it != m_windows.end();)
            {
                const auto found = observed.find(it->first);
                if (found == observed.end())
                {
                    it = m_windows.erase(it);
                    continue;
                }
                it->second.window = found->second;
                ++it;
            }
            m_saturated = false;
            // Roots first, so enumeration order cannot split an owned group.
            for (int pass = 0; pass < 2; ++pass)
            {
                for (const auto& window : snapshots)
                {
                    const bool root = window.id == window.root;
                    if (root != (pass == 0) || !window.manageable || !window.visible || window.desktop != desktop)
                        continue;
                    auto found = m_windows.find(window.id);
                    if (found == m_windows.end())
                    {
                        if (m_windows.size() >= MaxTrackedWindows)
                        {
                            m_saturated = true;
                            continue;
                        }
                        if (root)
                            found = m_windows.emplace(window.id, Entry{ window, window.monitor, Selected(desktop, window.monitor), false }).first;
                        else
                        {
                            const auto owner = m_windows.find(window.root);
                            if (owner == m_windows.end() || owner->second.window.desktop != desktop)
                                continue;
                            found = m_windows.emplace(window.id, Entry{ window, owner->second.assignedMonitor, owner->second.workspace, false }).first;
                        }
                    }
                    // A visible root dragged to another monitor joins its active group.
                    if (root && found->second.assignedMonitor != window.monitor && !found->second.hiddenByUs)
                    {
                        found->second.assignedMonitor = window.monitor;
                        found->second.workspace = Selected(desktop, window.monitor);
                    }
                    if (!root)
                    {
                        const auto owner = m_windows.find(window.root);
                        if (owner != m_windows.end())
                        {
                            found->second.assignedMonitor = owner->second.assignedMonitor;
                            found->second.workspace = owner->second.workspace;
                        }
                    }
                }
            }
            // Hidden owned members must follow the root's membership too; being
            // app-hidden does not make a cross-monitor dialog safe to ignore.
            for (auto& [id, member] : m_windows)
            {
                if (id == member.window.root)
                    continue;
                const auto owner = m_windows.find(member.window.root);
                if (owner != m_windows.end() && owner->second.window.desktop == member.window.desktop)
                {
                    member.assignedMonitor = owner->second.assignedMonitor;
                    member.workspace = owner->second.workspace;
                }
            }
        }
        unsigned Selected(const std::wstring& desktop, const std::wstring& monitor) const
        {
            const auto found = m_selected.find({ desktop, monitor });
            return found == m_selected.end() ? 0 : found->second;
        }
        std::optional<unsigned> Assignment(const WindowId& id) const
        {
            const auto found = m_windows.find(id);
            return found == m_windows.end() ? std::nullopt : std::optional<unsigned>(found->second.workspace);
        }
        Transition Select(const std::wstring& desktop, const std::wstring& monitor, unsigned selection) const
        {
            return Plan(desktop, monitor, selection);
        }
        Transition Step(const std::wstring& desktop, const std::wstring& monitor, int direction) const
        {
            if (direction != -1 && direction != 1)
                return {};
            const unsigned current = Selected(desktop, monitor);
            const unsigned next = direction < 0 ? (current + WorkspaceCount - 1) % WorkspaceCount : (current + 1) % WorkspaceCount;
            return Select(desktop, monitor, next);
        }
        Transition Move(const WindowId& id, int direction) const
        {
            const auto found = m_windows.find(id);
            if (found == m_windows.end() || (direction != -1 && direction != 1))
                return {};
            const auto& entry = found->second;
            const unsigned current = Selected(entry.window.desktop, entry.assignedMonitor);
            const unsigned next = direction < 0 ? (current + WorkspaceCount - 1) % WorkspaceCount : (current + 1) % WorkspaceCount;
            std::map<WindowId, unsigned> assignments;
            for (const auto& [memberId, member] : m_windows)
                if (member.window.root == entry.window.root && member.window.desktop == entry.window.desktop)
                    assignments.emplace(memberId, next);
            return Plan(entry.window.desktop, entry.assignedMonitor, std::nullopt, assignments);
        }
        bool Commit(const Transition& transition)
        {
            if (!transition.valid() || transition.version != m_version)
                return false;
            if (transition.selection)
                m_selected[{ transition.desktop, transition.monitor }] = *transition.selection;
            for (const auto& [id, workspace] : transition.assignments)
                m_windows.at(id).workspace = workspace;
            for (const auto& id : transition.hide)
                m_windows.at(id).hiddenByUs = true;
            for (const auto& id : transition.show)
                m_windows.at(id).hiddenByUs = false;
            ++m_version;
            return true;
        }
        void ReleaseVisibility()
        {
            for (auto& [id, entry] : m_windows)
                entry.hiddenByUs = false;
            ++m_version;
        }
        void Reset()
        {
            m_windows.clear();
            m_selected.clear();
            m_saturated = false;
            ++m_version;
        }
        std::size_t size() const { return m_windows.size(); }
    };
}
