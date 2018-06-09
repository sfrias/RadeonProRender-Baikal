/**********************************************************************
Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#include "material_explorer.h"
#include <memory>

static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{ return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }

static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs)
{ return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }

////////////////////////////////////////////////////////
//// MaterialExplorer implementation
////////////////////////////////////////////////////////

namespace
{
    struct MaterialExplorerConcrete : public MaterialExplorer
    {
        MaterialExplorerConcrete(UberV2Material::Ptr material) :
            MaterialExplorer(material)
        {   }
    };
}

MaterialExplorer::Ptr MaterialExplorer::Create(UberV2Material::Ptr material)
{
    return std::make_shared<MaterialExplorer>(MaterialExplorerConcrete(material));
}

MaterialExplorer::MaterialExplorer(UberV2Material::Ptr material) :
    m_material(material)
{
    auto layers = m_material->GetLayers();
    auto layers_desc = GetUberLayersDesc();

    auto find_layer =
        [&layers_desc](UberV2Material::Layers layers)
        {
            return *(std::find_if(
                layers_desc.begin(), layers_desc.end(),
                [layers](LayerDesc desc)
                {
                    if (static_cast<std::uint32_t>(desc.first) == layers)
                        return true;
                    return false;
                }));
        };

    // collect all supported layers
    if (layers & UberV2Material::kEmissionLayer)
        m_layers.push_back(find_layer(UberV2Material::kEmissionLayer));
    if (layers & UberV2Material::kTransparencyLayer)
        m_layers.push_back(find_layer(UberV2Material::kTransparencyLayer));
    if (layers & UberV2Material::kCoatingLayer)
        m_layers.push_back(find_layer(UberV2Material::kCoatingLayer));
    if (layers & UberV2Material::kReflectionLayer)
        m_layers.push_back(find_layer(UberV2Material::kReflectionLayer));
    if (layers & UberV2Material::kDiffuseLayer)
        m_layers.push_back(find_layer(UberV2Material::kDiffuseLayer));
    if (layers & UberV2Material::kRefractionLayer)
        m_layers.push_back(find_layer(UberV2Material::kRefractionLayer));
    if (layers & UberV2Material::kSSSLayer)
        m_layers.push_back(find_layer(UberV2Material::kSSSLayer));
    if (layers & UberV2Material::kShadingNormalLayer)
        m_layers.push_back(find_layer(UberV2Material::kShadingNormalLayer));
}

void MaterialExplorer::DrawExplorer(ImVec2 win_size)
{
    const float NODE_SLOT_RADIUS = 4.0f;
    const ImVec2 NODE_WINDOW_PADDING(8.0f, 8.0f);
    const int left_side_width = 150;

    // static variables
    static bool show_grid = true;
    static int input_selected = -1;
    static ImVec2 scrolling = ImVec2(0.0f, 0.0f);

    ImGui::SetNextWindowSize(win_size, ImGuiSetCond_FirstUseEver);

    // Draw a list of layers on the left side
    ImGui::BeginChild("layers_list", ImVec2((float)left_side_width, 0));
    ImGui::Text("Layers:");
    ImGui::Separator();

    int selected_id = 0;
    std::string input_name;
    for (const auto& layer : m_layers)
    {
        for (const auto& input : layer.second)
        {
            if (ImGui::Selectable(input.c_str(), selected_id == input_selected))
            {
                input_selected = selected_id;
                input_name = input;
            }
            selected_id++;
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginGroup();

    // draw canvas with inputs graph
    ImGui::Text("Hold middle mouse button to scroll (%.2f,%.2f)", scrolling.x, scrolling.y);
    ImGui::SameLine(ImGui::GetWindowWidth() - left_side_width);
    ImGui::Checkbox("Show grid", &show_grid);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImColor(IM_COL32(60, 60, 70, 200)));
    ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
    // ImGui::PushItemWidth(120.0f);

    if (!input_name.empty() &&
        m_selected_input != input_name)
    {
        auto input_map = m_material->GetInputValue(input_name).input_map_value;
        m_graph = GraphScheme::Create(UberTree::Create(input_map), RadeonRays::int2(left_side_width + 10, 100));
        m_selected_input = input_name;
    }

    ImVec2 offset = ImGui::GetCursorScreenPos() + scrolling;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // show grid
    if (show_grid)
    {
        ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
        float GRID_SZ = 64.0f;
        ImVec2 win_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_sz = ImGui::GetWindowSize();
        for (float x = fmodf(scrolling.x, GRID_SZ); x < canvas_sz.x; x += GRID_SZ)
            draw_list->AddLine(ImVec2(x, 0.0f) + win_pos, ImVec2(x, canvas_sz.y) + win_pos, GRID_COLOR);
        for (float y = fmodf(scrolling.y, GRID_SZ); y < canvas_sz.y; y += GRID_SZ)
            draw_list->AddLine(ImVec2(0.0f, y) + win_pos, ImVec2(canvas_sz.x, y) + win_pos, GRID_COLOR);
    }


    // Display nodes
    if (m_graph)
    {
        // Display links
        draw_list->ChannelsSplit(2);
        draw_list->ChannelsSetCurrent(0); // Background

        auto nodes = m_graph->GetNodes();

        for (const auto& node : nodes)
        {
            auto top_left_corner = ImVec2((float)node.pos.x, (float)node.pos.y) + offset;
            ImVec2 bottom_right_corner(
                (float)(top_left_corner.x + node.size.x),
                (float)(top_left_corner.y + node.size.y));

            ImGui::SetCursorScreenPos(top_left_corner);
            ImGui::InvisibleButton("node", ImVec2(node.size.x, node.size.y));

            bool node_hovered_in_scene = false;
            if (ImGui::IsItemHovered())
            {
                node_hovered_in_scene = true;
            }

            ImU32 node_bg_color = node_hovered_in_scene ? IM_COL32(75, 75, 75, 255) : IM_COL32(60, 60, 60, 255); // : IM_COL32(60, 60, 60, 255);

            draw_list->AddRectFilled(top_left_corner, bottom_right_corner, node_bg_color, 4.0f);
            draw_list->AddRect(top_left_corner, bottom_right_corner, IM_COL32(100, 100, 100, 255), 4.0f);

            // enable node drag
            bool node_moving_active = ImGui::IsItemActive();

            if (node_moving_active && ImGui::IsMouseDragging(0))
            {
                auto delta = ImGui::GetIO().MouseDelta;
                auto new_pos = RadeonRays::int2(
                    node.pos.x + (int)delta.x,
                    node.pos.y + (int)delta.y);
                m_graph->UpdateNodePos(node.id, new_pos);
            }
        }

    }

    // Scrolling
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(2, 0.0f))
        scrolling = scrolling + ImGui::GetIO().MouseDelta;

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::EndGroup();
}

std::vector<MaterialExplorer::LayerDesc> MaterialExplorer::GetUberLayersDesc()
{
    return std::vector<LayerDesc>
    {
        {
            UberV2Material::Layers::kEmissionLayer,
            { "uberv2.emission.color" }
        },
        {
            UberV2Material::Layers::kCoatingLayer,
            { "uberv2.coating.color", "uberv2.coating.ior" }
        },
        {
            UberV2Material::Layers::kReflectionLayer,
            {
                "uberv2.reflection.color",
                "uberv2.reflection.roughness",
                "uberv2.reflection.anisotropy",
                "uberv2.reflection.anisotropy_rotation",
                "uberv2.reflection.ior",
                "uberv2.reflection.metalness",
            }
        },
        {
            UberV2Material::Layers::kDiffuseLayer,
            { "uberv2.diffuse.color" }
        },
        {
            UberV2Material::Layers::kRefractionLayer,
            {
                "uberv2.refraction.color",
                "uberv2.refraction.roughness",
                "uberv2.refraction.ior"
            }
        },
        {
            UberV2Material::Layers::kTransparencyLayer,
            { "uberv2.transparency" }
        },
        {
            UberV2Material::Layers::kShadingNormalLayer,
            { "uberv2.shading_normal" }
        },
        {
            UberV2Material::Layers::kSSSLayer,
            {
                "uberv2.sss.absorption_color",
                "uberv2.sss.scatter_color",
                "uberv2.sss.subsurface_color",
                "uberv2.sss.absorption_distance",
                "uberv2.sss.scatter_distance",
                "uberv2.sss.scatter_direction"
            }
        },
    };
}

////////////////////////////////////////////////////////
// MaterialExplorer::Node implementation
////////////////////////////////////////////////////////

MaterialExplorer::Node::Node(
    int id_, const std::string& name_,
    const ImVec2& pos_, float value_,
    const ImVec4& color_, int inputs_count_, int outputs_count_) :
    id(id_), name(name_), pos(pos_), value(value_),color(color_),
    inputs_count(inputs_count_), outputs_count(outputs_count_)
{  }

ImVec2 MaterialExplorer::Node::GetInputSlotPos(int slot_no) const
{ 
    return ImVec2(pos.x, pos.y + size.y * ((float)slot_no + 1) / ((float)inputs_count + 1));
}

ImVec2 MaterialExplorer::Node::GetOutputSlotPos(int slot_no) const
{
    return ImVec2(pos.x + size.x, pos.y + size.y * ((float)slot_no + 1) / ((float)outputs_count + 1));
}


////////////////////////////////////////////////////////
// MaterialExplorer::NodeLink implementation
////////////////////////////////////////////////////////

MaterialExplorer::NodeLink::NodeLink(int input_id_, int input_slot_, int output_id_, int output_slot_) :
    input_id(input_id_), input_slot(input_slot_), output_id(output_id_), output_slot(output_slot_)
{   }