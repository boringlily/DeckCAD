#pragma once
#include "clay.h"
#include <string>
#include <tuple>

namespace UI {

class Component {
public:
    Component(std::string id)
    {
        clayId = CLAY_SID(Clay_String(id.data()));
    };

    void Draw()
    {
        Clay__OpenElement();
        UpdateConfig();
        Clay__ConfigureOpenElementPtr(&elementConfig);
        UpdateInternal();
        Clay__CloseElement();
    };

protected:
    // Update the element config if configurations need to dynamically update.
    void virtual UpdateConfig() {};

    // Process internal state such as drawing child components.
    void virtual UpdateInternal() {};

    inline Clay_BoundingBox GetBoundingBox() const
    {
        auto clayData = Clay_GetElementData(clayId);
        return clayData.boundingBox;
    }

    inline Clay_Dimensions GetDimensions() const
    {
        auto boundingBox = GetBoundingBox();
        return Clay_Dimensions { boundingBox.width, boundingBox.height };
    }

    /// @brief Check if the size of the current component has changed.
    /// @param previousDimension Pass in a statefull reference for what the last size was, value is updated if size did change.
    /// @return true if the previous dimension is different from the new one.
    inline bool CheckDimensionChanged(Clay_Dimensions& previousDimension)
    {
        Clay_Dimensions newSize = GetDimensions();
        bool changed { previousDimension.width != newSize.width || previousDimension.height != newSize.height };
        if (changed) {
            previousDimension = newSize;
        }
        return changed;
    }

    Clay_ElementId clayId;
    Clay_ElementDeclaration elementConfig;
};

};