// ps5-homebrew-ui - Native PS5 game library interface preview.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The records below are deliberately sample data. This screen exercises the
// native controller and presentation flow; it does not scan or install files.

#include "concepts/concepts.hpp"

#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

const Color kInk = Color::rgb(0xf2f4f8);
const Color kMuted = Color::rgb(0xa4adbc);
const Color kPanel = Color::rgb(0x171c25);
const Color kPanelLift = Color::rgb(0x202938);
const Color kAccent = Color::rgb(0x71b7ff);
const Color kGreen = Color::rgb(0x91d7ad);
const Color kAmber = Color::rgb(0xffc96b);
const Color kLine = Color::rgb(0x344153);

enum class PackageTab
{
    game,
    dlc,
    update,
};

struct DemoTitle
{
    const char *name;
    const char *id;
    const char *region;
    const char *size;
    const char *format;
    const char *state;
    const char *game_file;
    const char *dlc_file;
    const char *update_file;
};

constexpr std::array<DemoTitle, 8> kTitles{{
    {"ASTRO BOT", "PPSA03974", "USA", "65.4 GB", ".ffpkg", "Installed",
     "PPSA03974-game-v01.000.ffpkg", "PPSA03974-dlc-01.ffpkg", "PPSA03974-update-01.ffpkg"},
    {"GHOST OF TSUSHIMA", "PPSA02225", "USA", "61.8 GB", ".exfat", "Library",
     "PPSA02225-game.exfat", "PPSA02225-dlc-01.exfat", "PPSA02225-update-02.exfat"},
    {"GRAN TURISMO 7", "PPSA01316", "EUR", "111.2 GB", ".ffpkg", "Installed",
     "PPSA01316-game.ffpkg", "PPSA01316-dlc-01.ffpkg", "PPSA01316-update-03.ffpkg"},
    {"RETURNAL", "PPSA012 a", "USA", "56.1 GB", ".ffpkg", "Library",
     "PPSA012 a-game.ffpkg", "PPSA012 a-dlc-01.ffpkg", "PPSA012 a-update-01.ffpkg"},
    {"RATCHET & CLANK", "PPSA01474", "USA", "33.7 GB", ".exfat", "Installed",
     "PPSA01474-game.exfat", "PPSA01474-dlc-01.exfat", "PPSA01474-update-02.exfat"},
    {"DEMON'S SOULS", "PPSA01341", "EUR", "66.9 GB", ".ffpkg", "Library",
     "PPSA01341-game.ffpkg", "PPSA01341-dlc-01.ffpkg", "PPSA01341-update-01.ffpkg"},
    {"SACKBOY", "PPSA01289", "USA", "30.2 GB", ".ffpkg", "Installed",
     "PPSA01289-game.ffpkg", "PPSA01289-dlc-01.ffpkg", "PPSA01289-update-01.ffpkg"},
    {"RETURNAL: ASCENSION", "PPSA012 a", "EUR", "4.8 GB", ".ffpkg", "Library",
     "PPSA012 a-game.ffpkg", "PPSA012 a-dlc-02.ffpkg", "PPSA012 a-update-02.ffpkg"},
}};

constexpr const char *kTechniques[] = {
    "Native OpenGL screen with controller navigation and explicit focus",
    "Library and installed filters, cover and block layouts, and title details",
    "Package tabs show title IDs, filenames, extensions, and illustrative sizes",
    "Source settings expose the planned /hostapp, PS5 folder, and NFS choices",
    "Preview data and unavailable install actions are labelled honestly",
};

class Library final : public app::Concept
{
  public:
    explicit Library(app::Context &context) : context_(context)
    {
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "library",
            "PS5 Library",
            "A native, controller-first game library interface",
            "src/concepts/library.cpp",
            audio::SoundSet::glass,
            kAccent,
            kTechniques,
        };
        return kInfo;
    }

    void enter() override
    {
        age_ = 0.0f;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        if (source_open_)
        {
            if (input.is_pressed(Action::back))
            {
                source_open_ = false;
                feedback.play(audio::Cue::back);
                return;
            }
            if (input.nav == Direction::up || input.nav == Direction::down)
            {
                const int delta = input.nav == Direction::down ? 1 : -1;
                const int next = std::clamp(source_focus_ + delta, 0, 2);
                if (next != source_focus_)
                {
                    source_focus_ = next;
                    feedback.play(audio::Cue::focus);
                }
            }
            if (input.is_pressed(Action::confirm))
            {
                notice_ = "Source connector is not available in this preview";
                feedback.play(audio::Cue::error);
            }
            return;
        }

        if (detail_ >= 0)
        {
            if (input.is_pressed(Action::back))
            {
                detail_ = -1;
                feedback.play(audio::Cue::back);
                return;
            }
            if (input.nav == Direction::left || input.nav == Direction::right)
            {
                const int delta = input.nav == Direction::right ? 1 : -1;
                const int next = std::clamp(tab_ + delta, 0, 2);
                if (next != tab_)
                {
                    tab_ = next;
                    feedback.play(audio::Cue::focus);
                }
            }
            if (input.is_pressed(Action::confirm))
            {
                notice_ = "Install backend is not connected";
                feedback.play(audio::Cue::error);
            }
            return;
        }

        if (input.is_pressed(Action::west))
        {
            source_open_ = true;
            source_focus_ = 0;
            feedback.play(audio::Cue::open);
            return;
        }
        if (input.is_pressed(Action::menu))
        {
            installed_only_ = !installed_only_;
            focus_ = 0;
            feedback.play(audio::Cue::toggle);
        }
        if (input.is_pressed(Action::north))
        {
            blocks_ = !blocks_;
            feedback.play(audio::Cue::toggle);
        }
        if (input.nav != Direction::none)
        {
            const int delta = input.nav == Direction::right || input.nav == Direction::down ? 1 : -1;
            const int next = std::clamp(focus_ + delta, 0, static_cast<int>(kTitles.size()) - 1);
            if (next != focus_)
            {
                focus_ = next;
                feedback.play(audio::Cue::focus);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            detail_ = focus_;
            tab_ = static_cast<int>(PackageTab::game);
            notice_.clear();
            feedback.play(audio::Cue::select);
        }
    }

    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::gradient;
        frame.backdrop.colors[0] = Color::rgb(0x090d13);
        frame.backdrop.colors[1] = Color::rgb(0x182336);
        frame.backdrop.time = age_;

        gfx::DrawList &list = frame.scene;
        const ui::Fonts &fonts = context_.fonts;
        ui::text(list, fonts.display, "PS5 Library", 96.0f, 128.0f, 62.0f, kInk);
        ui::text(list, fonts.regular, "NATIVE UI PREVIEW  ·  SAMPLE RECORDS", 100.0f, 170.0f,
                 18.0f, kAmber, gfx::Align::left, 2.2f);
        ui::text(list, fonts.regular, installed_only_ ? "INSTALLED GAMES" : "LIBRARY BROWSER",
                 100.0f, 232.0f, 21.0f, kAccent, gfx::Align::left, 2.4f);
        ui::text(list, fonts.regular, "Source: /hostapp  ·  cached index: preview only",
                 100.0f, 270.0f, 22.0f, kMuted);

        if (detail_ >= 0)
            draw_detail(list, fonts);
        else
            draw_library(list, fonts);

        static constexpr ui::Hint kHints[] = {
            {ui::Button::cross, "Open"},
            {ui::Button::circle, "Back"},
            {ui::Button::square, "Sources"},
            {ui::Button::triangle, "Cover / blocks"},
            {ui::Button::options, "Installed / library"},
        };
        ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), kHints,
                       static_cast<int>(std::size(kHints)), 100.0f, false);
    }

  private:
    void draw_library(gfx::DrawList &list, const ui::Fonts &fonts) const
    {
        constexpr int kColumns = 4;
        constexpr float x0 = 100.0f;
        constexpr float y0 = 326.0f;
        constexpr float gap_x = 24.0f;
        constexpr float gap_y = 28.0f;
        constexpr float card_w = 408.0f;
        constexpr float card_h = 282.0f;

        for (std::size_t i = 0; i < kTitles.size(); ++i)
        {
            const int col = static_cast<int>(i) % kColumns;
            const int row = static_cast<int>(i) / kColumns;
            const float x = x0 + static_cast<float>(col) * (card_w + gap_x);
            const float y = y0 + static_cast<float>(row) * (card_h + gap_y);
            const bool focused = static_cast<int>(i) == focus_;
            const Rect card{x, y, card_w, card_h};
            list.rounded_rect(card, 18.0f, focused ? kPanelLift : kPanel,
                              focused ? 3.0f : 1.0f, focused ? kAccent : kLine);
            const Color cover = cover_color(i);
            if (blocks_)
            {
                list.rounded_rect({x + 22.0f, y + 22.0f, 84.0f, 84.0f}, 16.0f, cover);
                ui::text(list, fonts.semibold, initials(kTitles[i].name), x + 64.0f, y + 76.0f,
                         27.0f, kInk, gfx::Align::center);
                ui::text(list, fonts.semibold, kTitles[i].name, x + 126.0f, y + 60.0f, 25.0f,
                         kInk);
                ui::text(list, fonts.mono, kTitles[i].id, x + 126.0f, y + 94.0f, 20.0f, kMuted);
                ui::text(list, fonts.regular, kTitles[i].state, x + 126.0f, y + 132.0f, 21.0f,
                         kGreen);
                ui::text(list, fonts.mono, kTitles[i].size, x + 28.0f, y + 232.0f, 21.0f,
                         kInk);
                ui::text(list, fonts.mono, kTitles[i].format, x + card_w - 28.0f, y + 232.0f,
                         21.0f, kAmber, gfx::Align::right);
            }
            else
            {
                list.rounded_rect({x + 20.0f, y + 18.0f, card_w - 40.0f, 154.0f}, 14.0f, cover);
                list.rounded_rect({x + 32.0f, y + 30.0f, card_w - 64.0f, 130.0f}, 10.0f,
                                  Color::rgb(0x0a1019, 0.45f));
                ui::text(list, fonts.semibold, initials(kTitles[i].name), x + card_w * 0.5f,
                         y + 112.0f, 35.0f, kInk, gfx::Align::center, 1.0f);
                ui::text(list, fonts.semibold, kTitles[i].name, x + 24.0f, y + 207.0f, 23.0f,
                         kInk);
                ui::text(list, fonts.mono, kTitles[i].id, x + 24.0f, y + 239.0f, 18.0f, kMuted);
                ui::text(list, fonts.mono, kTitles[i].size, x + card_w - 20.0f, y + 239.0f, 19.0f,
                         kInk, gfx::Align::right);
                ui::text(list, fonts.regular, kTitles[i].state, x + 24.0f, y + 268.0f, 18.0f,
                         kGreen);
                ui::text(list, fonts.mono, kTitles[i].format, x + card_w - 20.0f, y + 268.0f,
                         18.0f, kAmber, gfx::Align::right);
            }
        }
        ui::text(list, fonts.regular, blocks_ ? "BLOCK VIEW" : "COVER VIEW", 1818.0f, 294.0f,
                 18.0f, kMuted, gfx::Align::right, 2.0f);
        if (!notice_.empty())
            ui::text(list, fonts.semibold, notice_, 960.0f, 968.0f, 24.0f, kAmber,
                     gfx::Align::center);
    }

    void draw_detail(gfx::DrawList &list, const ui::Fonts &fonts) const
    {
        const DemoTitle &title = kTitles[static_cast<std::size_t>(detail_)];
        list.rounded_rect({100.0f, 316.0f, 550.0f, 594.0f}, 24.0f, cover_color(detail_));
        list.rounded_rect({124.0f, 340.0f, 502.0f, 546.0f}, 18.0f,
                          Color::rgb(0x0b111a, 0.76f));
        ui::text(list, fonts.display, initials(title.name), 375.0f, 628.0f, 62.0f, kInk,
                 gfx::Align::center);
        ui::text(list, fonts.semibold, title.name, 704.0f, 374.0f, 42.0f, kInk);
        ui::text(list, fonts.mono, title.id, 704.0f, 420.0f, 24.0f, kAccent);
        ui::text(list, fonts.regular, "Region", 704.0f, 474.0f, 21.0f, kMuted);
        ui::text(list, fonts.regular, title.region, 824.0f, 474.0f, 21.0f, kInk);
        ui::text(list, fonts.regular, "Game size", 1000.0f, 474.0f, 21.0f, kMuted);
        ui::text(list, fonts.mono, title.size, 1126.0f, 474.0f, 21.0f, kInk);
        ui::text(list, fonts.regular, "Image format", 1340.0f, 474.0f, 21.0f, kMuted);
        ui::text(list, fonts.mono, title.format, 1512.0f, 474.0f, 21.0f, kAmber);

        static constexpr const char *kTabNames[] = {"GAME", "DLC", "UPDATE"};
        for (int i = 0; i < 3; ++i)
        {
            const Rect tab{704.0f + static_cast<float>(i) * 250.0f, 528.0f, 222.0f, 58.0f};
            list.rounded_rect(tab, 13.0f, i == tab_ ? kPanelLift : kPanel,
                              i == tab_ ? 2.0f : 1.0f, i == tab_ ? kAccent : kLine);
            ui::text(list, fonts.semibold, kTabNames[i], tab.cx(), 565.0f, 21.0f,
                     i == tab_ ? kInk : kMuted, gfx::Align::center, 2.0f);
        }

        const char *file = title.game_file;
        const char *kind = "BASE GAME";
        if (tab_ == static_cast<int>(PackageTab::dlc))
        {
            file = title.dlc_file;
            kind = "DLC PACKAGE";
        }
        else if (tab_ == static_cast<int>(PackageTab::update))
        {
            file = title.update_file;
            kind = "TITLE UPDATE";
        }
        list.rounded_rect({704.0f, 620.0f, 1016.0f, 168.0f}, 16.0f, kPanel, 1.0f, kLine);
        ui::text(list, fonts.regular, kind, 736.0f, 662.0f, 18.0f, kMuted,
                 gfx::Align::left, 2.0f);
        ui::text(list, fonts.mono, file, 736.0f, 706.0f, 21.0f, kInk);
        ui::text(list, fonts.mono, title.format, 736.0f, 752.0f, 21.0f, kAmber);
        ui::text(list, fonts.mono, tab_ == 0 ? title.size : "2.4 GB (sample)", 1668.0f, 752.0f,
                 21.0f, kInk, gfx::Align::right);
        list.rounded_rect({704.0f, 818.0f, 302.0f, 68.0f}, 15.0f, kPanelLift, 1.0f, kLine);
        ui::text(list, fonts.semibold, tab_ == 1 ? "INSTALL ALL" : "INSTALL",
                 855.0f, 862.0f, 21.0f, kMuted, gfx::Align::center);
        ui::text(list, fonts.regular, "Unavailable: no installer backend connected",
                 1032.0f, 862.0f, 21.0f, kAmber);
        ui::text(list, fonts.regular, "Preview package metadata; files have not been scanned",
                 704.0f, 930.0f, 20.0f, kMuted);
    }

    static Color cover_color(std::size_t index)
    {
        static constexpr std::uint32_t kColors[] = {
            0x365c88, 0x75515b, 0x76643e, 0x3e6d68, 0x53618d, 0x6a4d81, 0x527343, 0x8b563f};
        return Color::rgb(kColors[index % std::size(kColors)]);
    }

    static std::string initials(const char *name)
    {
        std::string result;
        bool next = true;
        for (const char *p = name; *p != '\0'; ++p)
        {
            if (*p == ' ' || *p == '&' || *p == ':')
            {
                next = true;
                continue;
            }
            if (next)
            {
                result.push_back(*p);
                next = false;
            }
        }
        return result.empty() ? "PS5" : result.substr(0, 3);
    }

    app::Context &context_;
    int focus_ = 0;
    int detail_ = -1;
    int tab_ = 0;
    int source_focus_ = 0;
    bool installed_only_ = false;
    bool blocks_ = false;
    bool source_open_ = false;
    float age_ = 0.0f;
    std::string notice_;
};

} // namespace

std::unique_ptr<app::Concept> make_library(app::Context &context)
{
    return std::make_unique<Library>(context);
}

} // namespace hui::concepts
