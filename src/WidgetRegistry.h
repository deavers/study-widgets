#pragma once

#include <QString>
#include <QWidget>

#include <functional>
#include <vector>

struct WidgetInfo
{
    QString id;
    QString displayName;
    std::function<QWidget*()> create;
};

inline std::vector<WidgetInfo>& widgetRegistry() 
{
    static std::vector<WidgetInfo> registry;
    return registry;
}

inline QWidget* createWidgetById(const QString& id) 
{
    for (const WidgetInfo& widget : widgetRegistry()) 
    {
        if (widget.id == id) 
        {
            return widget.create();
        }
    }

    return nullptr;
}

#define REGISTER_WIDGET(ID, DISPLAY_NAME, WIDGET_CLASS) \
    namespace { \
        const bool registered_##WIDGET_CLASS = []() { \
            widgetRegistry().push_back(WidgetInfo{ \
                ID, \
                DISPLAY_NAME, \
                []() -> QWidget* { \
                    return new WIDGET_CLASS(); \
                } \
            }); \
            return true; \
        }(); \
    }