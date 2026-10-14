// =============================================================================
// Assets — runtime loader for the procedural fantasy PNG assets
//
// Assets are drawn by tools/gen_assets.cpp (run `generate_assets`) into
// <source>/assets/{players,skills,cards}. The loader locates them at runtime
// (compile-time FUMBBL_ASSETS_DIR > <exe>/../assets > CWD/assets), caches the
// pixmaps, and falls back to a generic icon when a file is missing.
// =============================================================================
#pragma once

#include <QString>
#include <QPixmap>

namespace assets {

// Resolved assets directory (empty string when not found).
QString assetsDir();

// --- name -> slug maps (loaded from skill_names.txt / card_names.txt) -------
// Returns the slug for a skill/card NAME (case-insensitive); "generic" when
// the name is unknown (a generic icon is always drawn).
QString skillIconSlug(const QString &skillName);
QString cardIconSlug(const QString &cardName);

// --- cached pixmaps ----------------------------------------------------------
// Player bust: archetype 0 blocker, 1 runner, 2 blitzer, 3 catcher,
// 4 thrower; home = team color variant. Returns a null pixmap when no assets.
QPixmap playerBust(int archetype, bool home);
QPixmap skillPixmap(const QString &skillName);
QPixmap cardPixmap(const QString &cardName);

// True when at least the player busts are available.
bool isAvailable();

} // namespace assets
