#pragma once


#include <z2/UI/Widget.h>


namespace z2::UI {


struct GraphicsWidget : Widget {
    bool selected = false;
    bool selectable = false;
    bool draggable = false;
};


struct GraphicsView : Widget {
    std::set<GraphicsWidget *> children_selected;

    Point translate = {0, 0};

    virtual void select_child(GraphicsWidget *ptr, bool multiselect);
    void on_event(Event_Motion e) override;
    void on_event(Event_Mouse e) override;
    //void do_paint() override;
};


}  // namespace z2::UI
