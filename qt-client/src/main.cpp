// =============================================================================
// FUMBBL Reborn (Qt6) — Application entry point
// =============================================================================
#include <QApplication>
#include <QPalette>
#include <QTimer>

#include "ui/mainwindow.h"

namespace {

// Dark theme palette matching the FUMBBL aesthetic (dark background,
// green/gold team accents).
void applyDarkTheme(QApplication &app)
{
    app.setStyle("Fusion");

    QPalette pal;
    pal.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x26));
    pal.setColor(QPalette::WindowText, QColor(0xdc, 0xdc, 0xdc));
    pal.setColor(QPalette::Base, QColor(0x14, 0x14, 0x1a));
    pal.setColor(QPalette::AlternateBase, QColor(0x22, 0x22, 0x2c));
    pal.setColor(QPalette::ToolTipBase, QColor(0x2a, 0x2a, 0x34));
    pal.setColor(QPalette::ToolTipText, QColor(0xdc, 0xdc, 0xdc));
    pal.setColor(QPalette::Text, QColor(0xdc, 0xdc, 0xdc));
    pal.setColor(QPalette::Button, QColor(0x2a, 0x2a, 0x34));
    pal.setColor(QPalette::ButtonText, QColor(0xdc, 0xdc, 0xdc));
    pal.setColor(QPalette::Highlight, QColor(0x4a, 0x7c, 0x3f));
    pal.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(0x66, 0x66, 0x70));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x66, 0x66, 0x70));
    app.setPalette(pal);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("FUMBBL Reborn");
    QApplication::setOrganizationName("saruman");
    QApplication::setApplicationVersion("0.1.0");

    applyDarkTheme(app);

    MainWindow window;
    window.show();

    // --smoke: headless verification mode. Runs the UI offscreen for a short
    // while and quits with exit code 0 if the event loop stayed healthy.
    if (app.arguments().contains("--smoke")) {
        QTimer::singleShot(800, &app, &QApplication::quit);
    }

    return app.exec();
}
