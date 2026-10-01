#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPalette>

namespace aha
{
// ABYSSUS · VALLUM semantic tokens; both themes share geometry and behavior.
struct Theme {
    QColor background, secondaryBackground, surface, subtle, hover, border, strongBorder;
    QColor text, secondaryText, accent, strongAccent, focus, primaryButton, onPrimaryButton;
    QColor success{QStringLiteral("#6F8F83")}, warning{QStringLiteral("#B09464")};
    QColor danger{QStringLiteral("#A96F74")}, info{QStringLiteral("#607D99")};
};
const Theme &theme(bool dark);
QFont uiFont(int pixels = 14, bool medium = false, bool latin = false);
QPalette themePalette(bool dark);
QString themeStyleSheet(bool dark);
QString assetPath(const QString &name, bool dark);
QIcon uiIcon(const QString &name, bool dark);
void initializeUiResources();
} // namespace aha
