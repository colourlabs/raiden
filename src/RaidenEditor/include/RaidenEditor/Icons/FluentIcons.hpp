#pragma once

#include <QIcon>
#include <QString>

namespace RaidenEditor {

/// Loads Fluent UI System Icons from the built-in Qt resources (:/editor/icons/).
/// Usage:  auto icon = FluentIcon::load("Save");
class FluentIcon {
public:
  static QIcon load(const QString &iconName, int size = 20, bool filled = true);
};

} // namespace RaidenEditor
