#include <RaidenEditor/Icons/FluentIcons.hpp>

#include <QFileInfo>

namespace RaidenEditor {

QIcon FluentIcon::load(const QString &iconName, int size, bool filled) {
  // build path: :/editor/icons/<IconName>/ic_fluent_<snake>_<size>_<style>.svg
  QString style = filled ? QStringLiteral("filled") : QStringLiteral("regular");
  QString snake;
  for (int i = 0; i < iconName.size(); ++i) {
    QChar c = iconName[i];
    if (c.isUpper()) {
      if (i > 0) {
        snake += QLatin1Char('_');
      }

      snake += c.toLower();
    } else {
      snake += c;
    }
  }

  QString resourcePath =
      QStringLiteral(":/editor/icons/%1/ic_fluent_%2_%3_%4.svg")
          .arg(iconName, snake)
          .arg(size)
          .arg(style);

  if (!QFileInfo::exists(resourcePath)) {
    return {};
  }

  return QIcon(resourcePath);
}

} // namespace RaidenEditor
