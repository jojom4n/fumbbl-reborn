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

// Mockup F light theme: white/light-gray translucent surfaces, dark ink
// text, team accent colors, red reserved for injury/failure.
void applyLightTheme(QApplication &app)
{
    app.setStyle("Fusion");

    // Mockup F palette (see compose_f.py: PAPER / INK / INK2).
    const QColor paper(0xfa, 0xfa, 0xfd);
    const QColor ink(0x22, 0x28, 0x32);
    const QColor ink2(0x68, 0x72, 0x80);
    const QColor base(0xff, 0xff, 0xff);
    const QColor altBase(0xf2, 0xf2, 0xf6);
    const QColor button(0xf0, 0xf0, 0xf4);
    const QColor accent(0x4a, 0x7c, 0xba); // neutral blue selection

    QPalette pal;
    pal.setColor(QPalette::Window, paper);
    pal.setColor(QPalette::WindowText, ink);
    pal.setColor(QPalette::Base, base);
    pal.setColor(QPalette::AlternateBase, altBase);
    pal.setColor(QPalette::ToolTipBase, base);
    pal.setColor(QPalette::ToolTipText, ink);
    pal.setColor(QPalette::Text, ink);
    pal.setColor(QPalette::Button, button);
    pal.setColor(QPalette::ButtonText, ink);
    pal.setColor(QPalette::Highlight, accent);
    pal.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    pal.setColor(QPalette::Link, QColor(0x34, 0x6c, 0xb4));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, ink2);
    pal.setColor(QPalette::Disabled, QPalette::Text, ink2);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, ink2);
    app.setPalette(pal);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("FUMBBL Reborn");
    QApplication::setOrganizationName("saruman");
    // Single source of truth: the CMake project version (FUMBBL_VERSION).
    QApplication::setApplicationVersion(QStringLiteral(FUMBBL_VERSION));

    applyLightTheme(app);

    // gateLogin=true: the client login is the gate to the full UI (the
    // modal LoginDialog over the stadium; nothing is clickable — menus
    // included — until the server verifies the credentials).
    MainWindow window(nullptr, /*gateLogin=*/true);
    window.show();

    // --smoke: headless verification mode. Runs the UI offscreen for a short
    // while and quits with exit code 0 if the event loop stayed healthy.
    // --screenshot <file>: saves a PNG of the window before quitting (used
    // together with QT_QPA_PLATFORM=offscreen to verify the layout).
    // --demo <fixture.json>: load a captured serverGameState into the UI.
    //   Interactive by itself (the window stays open); combined with
    //   --screenshot it quits after capturing the image.
    bool smoke = app.arguments().contains("--smoke");
    // --spectate-now: open the Spectate Game dialog right after startup
    // (diagnostic / manual testing).
    bool spectateNow = app.arguments().contains("--spectate-now");
    QString screenshotPath;
    QString demoPath;
    const auto args = app.arguments();
    const int shotIdx = args.indexOf("--screenshot");
    if (shotIdx >= 0 && shotIdx + 1 < args.size())
        screenshotPath = args.at(shotIdx + 1);
    const int demoIdx = args.indexOf("--demo");
    if (demoIdx >= 0 && demoIdx + 1 < args.size())
        demoPath = args.at(demoIdx + 1);

    const auto loadDemo = [&window, demoPath]() {
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
        // Player Info panel renders real data.
        GameModel *m = window.model();
        if (m && m->isInitialized()) {
            QString id = m->activePlayerId();
            if (id.isEmpty() && !m->fieldTokens().isEmpty())
                id = m->fieldTokens().first().playerId;
            if (!id.isEmpty())
                window.showPlayerCardById(id);
        }
    };
    if (!demoPath.isEmpty())
        loadDemo();

    // Tool modes (--smoke / --screenshot / --demo) drive the UI directly
    // (fixtures, offscreen captures): no login gate.
    const bool toolMode = smoke || !screenshotPath.isEmpty() || !demoPath.isEmpty();
    if (!toolMode) {
        // The login gate: modal over the stadium. It returns only when the
        // server verified the credentials (dialog accepted) or the user
        // closed it (X / Esc / Quit). A closed gate ends the client: the
        // app.quit() emitted from inside the gate's own (nested) event
        // loop is NOT remembered by the main loop that starts afterwards,
        // so the quit decision is acted on here.
        if (!window.startLogin())
            return 0;
        if (spectateNow)
            window.openSpectateDialog();
    } else if (spectateNow) {
        QTimer::singleShot(800, &window, [&window]() { window.openSpectateDialog(); });
    }

    // --trigger-connect: emit the SAME signal a menu click emits (exact menu
    // path: connectionRequested -> onConnectRequested -> openSpectateDialog).
    if (app.arguments().contains("--trigger-connect"))
        QTimer::singleShot(800, &window, [&window]() {
            fprintf(stderr, "[spectate] --trigger-connect: emitting connectionRequested\n");
            emit window.connectionRequested();
        });

    if (smoke || !screenshotPath.isEmpty()) {
        // --size WxH (with --screenshot): use an explicit window size
        // instead of the offscreen screen's maximized default — the
        // window may be larger than the (small) offscreen screen.
        QString shotSize;
        const int sizeIdx = app.arguments().indexOf(QStringLiteral("--size"));
        if (sizeIdx >= 0 && sizeIdx + 1 < app.arguments().size())
            shotSize = app.arguments().at(sizeIdx + 1);
        QTimer::singleShot(100, &window, [&window, shotSize]() {
            if (shotSize.isEmpty())
                return;
            const auto parts = shotSize.split('x');
            if (parts.size() != 2)
                return;
            window.showNormal();
            window.resize(parts[0].toInt(), parts[1].toInt());
        });
        // --hide-panel <name> (repeatable): after the layout is applied.
        QStringList hiddenPanels;
        const auto &allArgs = app.arguments();
        for (int i = 1; i + 1 < allArgs.size(); ++i)
            if (allArgs[i] == QLatin1String("--hide-panel"))
                hiddenPanels << allArgs[i + 1];
        if (!hiddenPanels.isEmpty())
            QTimer::singleShot(600, &window, [&window, hiddenPanels]() {
                for (const QString &p : hiddenPanels)
                    window.setPanelVisible(p, false);
            });
        QTimer::singleShot(1200, &app, [&app, &window, screenshotPath]() {
            if (!screenshotPath.isEmpty())
                window.grab().save(screenshotPath);
            app.quit();
        });
    }

    return app.exec();
}
