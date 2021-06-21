#include <imgui.h>

#include "Array.hpp"
#include "Bitfield.hpp"
#include "Pointer.hpp"
#include "Undefined.hpp"

#include "Struct.hpp"

namespace node {
Struct::Struct(Process& process, genny::Variable* var, Property& props)
    : Variable{process, var, props}, m_struct{dynamic_cast<genny::Struct*>(var->type())} {
    assert(m_struct != nullptr);

    if (m_props["__collapsed"].value.index() == 0) {
        is_collapsed(true);
    }

    // Build the node map.
    auto make_node = [&](genny::Variable* var) -> std::unique_ptr<Base> {
        auto&& props = m_props[var->name()];

        if (var->type()->is_a<genny::Array>()) {
            return std::make_unique<Array>(m_process, var, props);
        } else if (var->type()->is_a<genny::Struct>()) {
            return std::make_unique<Struct>(m_process, var, props);
        } else if (var->type()->is_a<genny::Pointer>()) {
            return std::make_unique<Pointer>(m_process, var, props);
        } else if (var->is_bitfield()) {
            return std::make_unique<Bitfield>(m_process, var, props);
        } else {
            return std::make_unique<Variable>(m_process, var, props);
        }
    };
    std::function<void(genny::Struct*)> add_vars = [&](genny::Struct* s) {
        for (auto&& parent : s->parents()) {
            add_vars(parent);
        }

        for (auto&& var : s->get_all<genny::Variable>()) {
            m_nodes.emplace(var->offset(), make_node(var));
        }
    };

    add_vars(m_struct);

    // Fill in the rest of the offsets with undefined nodes.
    uintptr_t last_offset{};

    auto fill_space = [&](int delta) {
        switch (delta) {
        case 8:
            m_nodes.emplace(last_offset, std::make_unique<Undefined>(m_process, 8));
            break;

        case 7:
            m_nodes.emplace(last_offset, std::make_unique<Undefined>(m_process, 4));
            m_nodes.emplace(last_offset + 4, std::make_unique<Undefined>(m_process, 2));
            m_nodes.emplace(last_offset + 6, std::make_unique<Undefined>(m_process, 1));
            break;

        case 6:
            m_nodes.emplace(last_offset, std::make_unique<Undefined>(m_process, 4));
            m_nodes.emplace(last_offset + 4, std::make_unique<Undefined>(m_process, 2));
            break;

        case 5:
            m_nodes.emplace(last_offset, std::make_unique<Undefined>(m_process, 4));
            m_nodes.emplace(last_offset + 4, std::make_unique<Undefined>(m_process, 1));
            break;

        case 4:
            m_nodes.emplace(last_offset, std::make_unique<Undefined>(m_process, 4));
            break;

        case 3:
            m_nodes.emplace(last_offset, std::make_unique<Undefined>(m_process, 2));
            m_nodes.emplace(last_offset + 2, std::make_unique<Undefined>(m_process, 1));
            break;

        case 2:
            m_nodes.emplace(last_offset, std::make_unique<Undefined>(m_process, 2));
            break;

        case 1:
            m_nodes.emplace(last_offset, std::make_unique<Undefined>(m_process, 1));
            break;

        default:
            break;
        }
    };

    for (uintptr_t offset = 0; offset < m_size;) {
        auto delta = offset - last_offset;

        if (auto search = m_nodes.find(offset); search != m_nodes.end()) {
            auto& node = search->second;
            fill_space(delta);
            last_offset = offset + node->size();

            if (offset == last_offset) { // A type with 0 size was encountered.
                ++offset;
            } else {
                offset = last_offset;
            }
        } else if (delta > 0 && offset % sizeof(uintptr_t) == 0) {
            fill_space(delta);
            last_offset = offset;
            ++offset;
        } else {
            ++offset;
        }
    }

    fill_space(m_size - last_offset);
}

void Struct::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    if (m_display_self) {
        ImGui::BeginGroup();
        display_address_offset(address, offset);
        ImGui::SameLine();
        display_type();
        ImGui::SameLine();
        display_name();
        ImGui::EndGroup();

        if (ImGui::BeginPopupContextItem("ArrayNode")) {
            ImGui::Checkbox("Collpase", &is_collapsed());
            ImGui::EndPopup();
        }

        if (is_collapsed()) {
            return;
        }
    }

    for (auto&& [node_offset, node] : m_nodes) {
        if (m_display_self) {
            ++indentation_level;
        }

        ImGui::PushID(node.get());
        node->display(address + node_offset, offset + node_offset, &mem[node_offset]);
        ImGui::PopID();

        if (m_display_self) {
            --indentation_level;
        }
    }
}

void Struct::on_refresh(uintptr_t address, uintptr_t offset, std::byte* mem) {
    if (is_collapsed()) {
        return;
    }

    for (auto&& [node_offset, node] : m_nodes) {
        node->on_refresh(address + node_offset, offset + node_offset, &mem[node_offset]);
    }
}
} // namespace node
