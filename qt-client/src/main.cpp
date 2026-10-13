// =============================================================================
// FUMBBL Reborn (Qt6) — Application entry point
// =============================================================================
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QTimer>

#include "core/gamemodel.h"
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
    // --screenshot <file>: saves a PNG of the window before quitting (used
    // together with QT_QPA_PLATFORM=offscreen to verify the layout).
    bool smoke = app.arguments().contains("--smoke");
    QString screenshotPath;
    QString demoPath; // --demo <fixture.json>: load a serverGameState capture
    const auto args = app.arguments();
    const int shotIdx = args.indexOf("--screenshot");
    if (shotIdx >= 0 && shotIdx + 1 < args.size())
        screenshotPath = args.at(shotIdx + 1);
    const int demoIdx = args.indexOf("--demo");
    if (demoIdx >= 0 && demoIdx + 1 < args.size())
        demoPath = args.at(demoIdx + 1);

    if (smoke || !screenshotPath.isEmpty() || !demoPath.isEmpty()) {
        QTimer::singleShot(1200, &app, [&app, &window, screenshotPath, demoPath]() {
            if (!demoPath.isEmpty()) {
                QFile f(demoPath);
                if (f.open(QIODevice::ReadOnly)) {
                    const QJsonObject game =
                        QJsonDocument::fromJson(f.readAll())
                            .object()
                            .value(QStringLiteral("game"))
                            .toObject();
                    if (!game.isEmpty())
                        window.onGameState(game);
                }
                // Select the acting player (or the first on-field one) so the
                // Player Info panel renders real data in the screenshot.
                GameModel *m = window.model();
                if (m && m->isInitialized()) {
                    QString id = m->activePlayerId();
                    if (id.isEmpty() && !m->fieldTokens().isEmpty())
                        id = m->fieldTokens().first().playerId;
                    if (!id.isEmpty())
                        window.selectPlayer(id);
                }
            }
            if (!screenshotPath.isEmpty())
                window.grab().save(screenshotPath);
            app.quit();
        });
    }

    return app.exec();
}
