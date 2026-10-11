// ps5-homebrew-ui - Native PS5 game library.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Live titles come from ShadowMount Plus over loopback. A validated local
// snapshot can appear quickly, but the library stays locked until SMP responds.
// This UI never bundles or launches the ShadowMount Plus payload.

#include "concepts/concepts.hpp"

#include "core/save_file.hpp"
#include "library/shadowmount_client.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <pthread.h>
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
    {"RETURNAL", "PPSA01234", "USA", "56.1 GB", ".ffpkg", "Library",
     "PPSA01234-game.ffpkg", "PPSA01234-dlc-01.ffpkg", "PPSA01234-update-01.ffpkg"},
    {"RATCHET & CLANK", "PPSA01474", "USA", "33.7 GB", ".exfat", "Installed",
     "PPSA01474-game.exfat", "PPSA01474-dlc-01.exfat", "PPSA01474-update-02.exfat"},
    {"DEMON'S SOULS", "PPSA01341", "EUR", "66.9 GB", ".pkg", "Library",
     "PPSA01341-game.pkg", "PPSA01341-dlc-01.pkg", "PPSA01341-update-01.pkg"},
    {"SACKBOY", "PPSA01289", "USA", "30.2 GB", ".ffpkg", "Installed",
     "PPSA01289-game.ffpkg", "PPSA01289-dlc-01.ffpkg", "PPSA01289-update-01.ffpkg"},
    {"RETURNAL: ASCENSION", "PPSA01234", "EUR", "4.8 GB", ".ffpkg", "Library",
     "PPSA01234-game.ffpkg", "PPSA01234-dlc-02.ffpkg", "PPSA01234-update-02.ffpkg"},
}};

constexpr std::uint16_t kCatalogCacheVersion = 1;
constexpr std::size_t kMaxCatalogCacheBytes = 2u * 1024u * 1024u + 16u;

constexpr const char *kTechniques[] = {
    "Native OpenGL screen with controller navigation and explicit focus",
    "Library and installed filters, cover and block layouts, and title details",
    "ShadowMount Plus index with a validated per-title cache and runtime lock",
    "Loopback API work and cache writes run on a worker thread",
    "Package groups and install actions show explicit unavailable states",
};

class Library final : public app::Concept
{
  public:
    explicit Library(app::Context &context)
        : context_(context),
          cache_path_(context.data_root.empty() ? std::string()
                                                : context.data_root + "/library.cache")
    {
        load_cached_catalog();
    }

    ~Library() override
    {
        if (refresh_started_)
            pthread_join(refresh_thread_, nullptr);
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
        dependency_ready_ = false;
        begin_refresh();
    }

    std::span<const app::TourStep> tour() const override
    {
        static constexpr app::TourStep kTour[] = {
            {0.4f, 0, Direction::down, nullptr},
            {0.4f, action_bit(Action::confirm), Direction::none, nullptr},
            {0.4f, 0, Direction::right, nullptr},
            {0.4f, action_bit(Action::back), Direction::none, nullptr},
            {0.4f, action_bit(Action::west), Direction::none, nullptr},
            {0.4f, 0, Direction::down, nullptr},
            {0.4f, action_bit(Action::back), Direction::none, nullptr},
        };
        return kTour;
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        finish_refresh();
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
                if (source_focus_ == 0)
                {
                    begin_refresh();
                    feedback.play(audio::Cue::open);
                }
                else
                {
                    notice_ = source_focus_ == 1
                                  ? "PS5 folder picker is not implemented"
                                  : "NFS client is not implemented";
                    feedback.play(audio::Cue::error);
                }
            }
            return;
        }

        if (!dependency_ready_)
        {
            if (input.is_pressed(Action::west))
            {
                source_open_ = true;
                source_focus_ = 0;
                feedback.play(audio::Cue::open);
            }
            else if (input.is_pressed(Action::confirm))
            {
                begin_refresh();
                feedback.play(audio::Cue::open);
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
            focus_ = visible_count() == 0 ? 0 : std::min(focus_, visible_count() - 1);
            update_scroll();
            feedback.play(audio::Cue::toggle);
        }
        if (input.is_pressed(Action::north))
        {
            blocks_ = !blocks_;
            feedback.play(audio::Cue::toggle);
        }
        if (input.nav != Direction::none && visible_count() > 0)
        {
            const int delta = input.nav == Direction::right || input.nav == Direction::down ? 1 : -1;
            const int next = std::clamp(focus_ + delta, 0, visible_count() - 1);
            if (next != focus_)
            {
                focus_ = next;
                update_scroll();
                feedback.play(audio::Cue::focus);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            if (visible_count() > 0)
            {
                detail_ = focus_;
                tab_ = static_cast<int>(PackageTab::game);
                notice_.clear();
                feedback.play(audio::Cue::select);
            }
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
        ui::text(list, fonts.regular,
                 dependency_ready_ ? (catalog_from_cache_ ? "NATIVE LIBRARY  ·  SAVED SMP INDEX"
                                                          : "NATIVE LIBRARY  ·  SHADOWMOUNT INDEX")
                                   : "FEATURES LOCKED  ·  SHADOWMOUNT PLUS REQUIRED",
                 100.0f, 170.0f,
                 18.0f, kAmber, gfx::Align::left, 2.2f);
        ui::text(list, fonts.regular, installed_only_ ? "INSTALLED GAMES" : "LIBRARY BROWSER",
                 100.0f, 232.0f, 21.0f, kAccent, gfx::Align::left, 2.4f);
        ui::text(list, fonts.regular,
                 dependency_ready_ ? (catalog_from_cache_ ? "Source: saved local snapshot"
                                                          : "Source: ShadowMount Plus  ·  live indexed catalog")
                                   : "Source: locked until the local SMP API responds",
                 100.0f, 270.0f, 22.0f, kMuted);

        if (source_open_)
            draw_source(list, fonts);
        else if (!dependency_ready_)
            draw_dependency_lock(list, fonts);
        else if (detail_ >= 0)
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

        const int count = std::min(8, std::max(0, visible_count() - first_visible_));
        for (int row_index = 0; row_index < count; ++row_index)
        {
            const int visible = first_visible_ + row_index;
            const std::size_t i = static_cast<std::size_t>(visible_title(visible));
            char size[32];
            size_text(i, size, sizeof(size));
            const char *name = title_name(i);
            const char *id = title_id(i);
            const char *state = title_installed(i) ? "Installed" : "Library";
            const char *format = title_format(i);
            const int col = row_index % kColumns;
            const int row = row_index / kColumns;
            const float x = x0 + static_cast<float>(col) * (card_w + gap_x);
            const float y = y0 + static_cast<float>(row) * (card_h + gap_y);
            const bool focused = visible == focus_;
            const Rect card{x, y, card_w, card_h};
            list.bordered_rect(card, 18.0f, focused ? kPanelLift : kPanel,
                              focused ? 3.0f : 1.0f, focused ? kAccent : kLine);
            const Color cover = cover_color(i);
            if (blocks_)
            {
                list.rounded_rect({x + 22.0f, y + 22.0f, 84.0f, 84.0f}, 16.0f, cover);
                ui::text(list, fonts.semibold, initials(name), x + 64.0f, y + 76.0f,
                         27.0f, kInk, gfx::Align::center);
                ui::text(list, fonts.semibold, name, x + 126.0f, y + 60.0f, 25.0f,
                         kInk);
                ui::text(list, fonts.mono, id, x + 126.0f, y + 94.0f, 20.0f, kMuted);
                ui::text(list, fonts.regular, state, x + 126.0f, y + 132.0f, 21.0f,
                         kGreen);
                ui::text(list, fonts.mono, size, x + 28.0f, y + 232.0f, 21.0f,
                         kInk);
                ui::text(list, fonts.mono, format, x + card_w - 28.0f, y + 232.0f,
                         21.0f, kAmber, gfx::Align::right);
            }
            else
            {
                list.rounded_rect({x + 20.0f, y + 18.0f, card_w - 40.0f, 154.0f}, 14.0f, cover);
                list.rounded_rect({x + 32.0f, y + 30.0f, card_w - 64.0f, 130.0f}, 10.0f,
                                  Color::rgb(0x0a1019, 0.45f));
                ui::text(list, fonts.semibold, initials(name), x + card_w * 0.5f,
                         y + 112.0f, 35.0f, kInk, gfx::Align::center, 1.0f);
                ui::text(list, fonts.semibold, name, x + 24.0f, y + 207.0f, 23.0f,
                         kInk);
                ui::text(list, fonts.mono, id, x + 24.0f, y + 239.0f, 18.0f, kMuted);
                ui::text(list, fonts.mono, size, x + card_w - 20.0f, y + 239.0f, 19.0f,
                         kInk, gfx::Align::right);
                ui::text(list, fonts.regular, state, x + 24.0f, y + 268.0f, 18.0f,
                         kGreen);
                ui::text(list, fonts.mono, format, x + card_w - 20.0f, y + 268.0f,
                         18.0f, kAmber, gfx::Align::right);
            }
        }
        ui::text(list, fonts.regular, blocks_ ? "BLOCK VIEW" : "COVER VIEW", 1818.0f, 294.0f,
                 18.0f, kMuted, gfx::Align::right, 2.0f);
        if (visible_count() == 0)
            ui::text(list, fonts.semibold, "No titles match this view", 960.0f, 560.0f, 30.0f,
                     kMuted, gfx::Align::center);
        else if (visible_count() > 8)
        {
            char page[48];
            std::snprintf(page, sizeof(page), "%d–%d / %d", first_visible_ + 1,
                          std::min(first_visible_ + 8, visible_count()), visible_count());
            ui::text(list, fonts.mono, page, 1818.0f, 968.0f, 18.0f, kMuted, gfx::Align::right);
        }
        if (!notice_.empty())
            ui::text(list, fonts.semibold, notice_, 960.0f, 968.0f, 24.0f, kAmber,
                     gfx::Align::center);
    }

    void draw_dependency_lock(gfx::DrawList &list, const ui::Fonts &fonts) const
    {
        list.bordered_rect({384.0f, 326.0f, 1152.0f, 486.0f}, 24.0f, kPanel, 1.0f, kLine);
        ui::text(list, fonts.display, "ShadowMount Plus required", 960.0f, 410.0f, 42.0f,
                 kInk, gfx::Align::center);
        ui::text(list, fonts.regular,
                 "Start the existing ShadowMount Plus payload on this PS5 to unlock the library.",
                 960.0f, 472.0f, 25.0f, kMuted, gfx::Align::center);
        ui::text(list, fonts.regular,
                 "This native app connects to SMP; it does not install or launch another payload.",
                 960.0f, 520.0f, 22.0f, kMuted, gfx::Align::center);
        ui::text(list, fonts.semibold,
                 notice_.empty() ? "Checking the local ShadowMount Plus API..." : notice_.c_str(),
                 960.0f, 598.0f, 23.0f, kAmber, gfx::Align::center);
        ui::text(list, fonts.regular,
                 "Cross: retry connection     Square: prerequisite details",
                 960.0f, 672.0f, 21.0f, kAccent, gfx::Align::center);
        ui::text(list, fonts.mono, "127.0.0.1:10101  ·  loopback only",
                 960.0f, 744.0f, 19.0f, kMuted, gfx::Align::center);
    }

    void draw_source(gfx::DrawList &list, const ui::Fonts &fonts) const
    {
        list.bordered_rect({400.0f, 300.0f, 1120.0f, 590.0f}, 24.0f, kPanel, 1.0f, kLine);
        ui::text(list, fonts.display, "Library sources", 456.0f, 372.0f, 42.0f, kInk);
        ui::text(list, fonts.regular, "Choose where the native index will look", 456.0f, 412.0f,
                 23.0f, kMuted);
        static constexpr const char *kSources[] = {
            "ShadowMount Plus  ·  required local catalog service",
            "PS5 folder  ·  browse internal storage",
            "NFS share  ·  address, export, user, password",
        };
        for (int i = 0; i < 3; ++i)
        {
            const Rect row{456.0f, 456.0f + static_cast<float>(i) * 82.0f, 1008.0f, 62.0f};
            list.bordered_rect(row, 12.0f, source_focus_ == i ? kPanelLift : kPanel,
                              source_focus_ == i ? 2.0f : 1.0f,
                              source_focus_ == i ? kAccent : kLine);
            ui::text(list, fonts.regular, kSources[i], row.x + 24.0f, row.y + 39.0f, 23.0f,
                     source_focus_ == i ? kInk : kMuted);
        }
        ui::text(list, fonts.regular,
                 "Requirement: ShadowMount Plus payload installed and running on this PS5",
                 456.0f, 704.0f, 20.0f, kMuted);
        ui::text(list, fonts.mono, "Address: —     Export path: —     User: —     Password: —",
                 456.0f, 734.0f, 20.0f, kMuted);
        ui::text(list, fonts.regular,
                 notice_.empty() ? "Client only · loopback 127.0.0.1:10101 · does not start another payload"
                                 : notice_.c_str(),
                 456.0f, 786.0f, 21.0f, kAmber);
        ui::text(list, fonts.regular, "Confirm ShadowMount to refresh  ·  Circle to return",
                 456.0f, 844.0f, 20.0f, kMuted);
    }

    void load_cached_catalog()
    {
        if (cache_path_.empty())
            return;
        std::string data;
        if (!save::read_file(cache_path_, &data, kMaxCatalogCacheBytes))
            return;
        const save::Decoded decoded = save::decode(save::Kind::library, data);
        if (!decoded.ok || decoded.version != kCatalogCacheVersion)
            return;
        library::Catalog cached{};
        const library::ParseResult parsed = library::parse_shadowmount_games(
            decoded.payload.data(), decoded.payload.size(), cached);
        if (!parsed)
            return;
        catalog_ = cached;
        has_live_catalog_ = true;
        catalog_from_cache_ = true;
    }

    void begin_refresh()
    {
        if (cache_path_.empty())
        {
            notice_ = "ShadowMount Plus connection is available in the PS5 app";
            return;
        }
        if (refresh_started_ && !refresh_done_.load(std::memory_order_acquire))
            return;
        if (refresh_started_)
        {
            pthread_join(refresh_thread_, nullptr);
            refresh_started_ = false;
        }
        refresh_done_.store(false, std::memory_order_relaxed);
        notice_ = "Connecting to the installed ShadowMount Plus payload...";
        if (pthread_create(&refresh_thread_, nullptr, &Library::refresh_worker, this) != 0)
        {
            notice_ = "Could not start the ShadowMount refresh worker";
            return;
        }
        refresh_started_ = true;
    }

    static void *refresh_worker(void *opaque)
    {
        auto &self = *static_cast<Library *>(opaque);
        library::Catalog result_catalog{};
        self.pending_json_.clear();
        self.pending_cache_saved_ = false;
        self.refresh_result_ =
            library::fetch_shadowmount_games(result_catalog, &self.pending_json_);
        if (self.refresh_result_)
        {
            self.pending_catalog_ = result_catalog;
            if (!self.cache_path_.empty() && !self.pending_json_.empty())
            {
                const std::string encoded = save::encode(save::Kind::library,
                                                         kCatalogCacheVersion,
                                                         self.pending_json_);
                self.pending_cache_saved_ = save::write_atomic(self.cache_path_, encoded).empty();
            }
        }
        self.refresh_done_.store(true, std::memory_order_release);
        return nullptr;
    }

    void finish_refresh()
    {
        if (!refresh_done_.load(std::memory_order_acquire) || !refresh_started_)
            return;
        pthread_join(refresh_thread_, nullptr);
        refresh_started_ = false;
        if (refresh_result_)
        {
            catalog_ = pending_catalog_;
            has_live_catalog_ = true;
            catalog_from_cache_ = false;
            dependency_ready_ = true;
            focus_ = 0;
            first_visible_ = 0;
            char message[128];
            std::snprintf(message, sizeof(message), "%zu titles indexed%s", catalog_.count,
                          pending_cache_saved_ ? " · saved for next launch" : "");
            notice_ = message;
        }
        else
        {
            notice_ = refresh_result_.error == library::FetchError::connect_failed
                          ? (has_live_catalog_
                                 ? "ShadowMount Plus is not running; saved list stays locked"
                                 : "ShadowMount Plus is not running — start its payload, then retry")
                          : "ShadowMount Plus API check failed; verify the payload and retry";
        }
    }

    int visible_count() const
    {
        if (has_live_catalog_)
        {
            if (!installed_only_)
            {
                int count = 0;
                for (std::size_t i = 0; i < catalog_.count; ++i)
                    count += catalog_.games[i].is_ps5 ? 1 : 0;
                return count;
            }
            int count = 0;
            for (std::size_t i = 0; i < catalog_.count; ++i)
                count += catalog_.games[i].is_ps5 && catalog_.games[i].installed ? 1 : 0;
            return count;
        }
        if (!installed_only_)
            return static_cast<int>(kTitles.size());
        int count = 0;
        for (const DemoTitle &title : kTitles)
            count += title.state[0] == 'I' ? 1 : 0;
        return count;
    }

    void update_scroll()
    {
        if (focus_ < first_visible_)
            first_visible_ = focus_;
        else if (focus_ >= first_visible_ + 8)
            first_visible_ = focus_ - 7;
        first_visible_ = std::max(0, std::min(first_visible_, std::max(0, visible_count() - 8)));
    }

    int visible_title(int visible) const
    {
        if (has_live_catalog_)
        {
            if (!installed_only_)
            {
                for (std::size_t i = 0; i < catalog_.count; ++i)
                {
                    if (!catalog_.games[i].is_ps5)
                        continue;
                    if (visible-- == 0)
                        return static_cast<int>(i);
                }
                return 0;
            }
            for (std::size_t i = 0; i < catalog_.count; ++i)
            {
                if (!catalog_.games[i].is_ps5 || !catalog_.games[i].installed)
                    continue;
                if (visible-- == 0)
                    return static_cast<int>(i);
            }
            return 0;
        }
        if (!installed_only_)
            return visible;
        for (std::size_t i = 0; i < kTitles.size(); ++i)
        {
            if (kTitles[i].state[0] != 'I')
                continue;
            if (visible-- == 0)
                return static_cast<int>(i);
        }
        return 0;
    }

    const char *title_name(std::size_t index) const
    {
        return has_live_catalog_ ? catalog_.games[index].name : kTitles[index].name;
    }

    const char *title_id(std::size_t index) const
    {
        return has_live_catalog_ ? catalog_.games[index].title_id : kTitles[index].id;
    }

    bool title_installed(std::size_t index) const
    {
        return has_live_catalog_ ? catalog_.games[index].installed
                                 : kTitles[index].state[0] == 'I';
    }

    const char *title_format(std::size_t index) const
    {
        if (!has_live_catalog_)
            return kTitles[index].format;
        const library::Game &game = catalog_.games[index];
        if (std::strcmp(game.source_type, "folder") == 0)
            return "FOLDER";
        if (std::strcmp(game.source_type, "pkg") == 0)
            return ".pkg";
        const char *dot = std::strrchr(game.path, '.');
        const char *slash = std::strrchr(game.path, '/');
        return dot && (!slash || dot > slash) ? dot : "IMAGE";
    }

    void size_text(std::size_t index, char *out, std::size_t capacity) const
    {
        if (!has_live_catalog_)
        {
            std::snprintf(out, capacity, "%s", kTitles[index].size);
            return;
        }
        const library::Game &game = catalog_.games[index];
        if (!game.has_size)
        {
            std::snprintf(out, capacity, "Size unavailable");
            return;
        }
        constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
        std::snprintf(out, capacity, "%.1f GB", static_cast<double>(game.size_bytes) / kGiB);
    }

    void draw_live_detail(gfx::DrawList &list, const ui::Fonts &fonts) const
    {
        const std::size_t index = static_cast<std::size_t>(visible_title(detail_));
        const library::Game &game = catalog_.games[index];
        char size[32];
        size_text(index, size, sizeof(size));
        list.rounded_rect({100.0f, 316.0f, 550.0f, 594.0f}, 24.0f, cover_color(index));
        list.rounded_rect({124.0f, 340.0f, 502.0f, 546.0f}, 18.0f,
                          Color::rgb(0x0b111a, 0.76f));
        ui::text(list, fonts.display, initials(game.name), 375.0f, 628.0f, 62.0f, kInk,
                 gfx::Align::center);
        ui::text(list, fonts.semibold, game.name, 704.0f, 374.0f, 42.0f, kInk);
        ui::text(list, fonts.mono, game.title_id, 704.0f, 420.0f, 24.0f, kAccent);
        ui::text(list, fonts.regular, "Version", 704.0f, 474.0f, 21.0f, kMuted);
        ui::text(list, fonts.regular, game.version[0] ? game.version : "Unknown", 824.0f,
                 474.0f, 21.0f, kInk);
        ui::text(list, fonts.regular, "Game size", 1000.0f, 474.0f, 21.0f, kMuted);
        ui::text(list, fonts.mono, size, 1126.0f, 474.0f, 21.0f, kInk);
        ui::text(list, fonts.regular, "Source format", 1340.0f, 474.0f, 21.0f, kMuted);
        ui::text(list, fonts.mono, title_format(index), 1512.0f, 474.0f, 21.0f, kAmber);

        static constexpr const char *kTabNames[] = {"GAME", "DLC", "UPDATE"};
        for (int i = 0; i < 3; ++i)
        {
            const Rect tab{704.0f + static_cast<float>(i) * 250.0f, 528.0f, 222.0f, 58.0f};
            list.bordered_rect(tab, 13.0f, i == tab_ ? kPanelLift : kPanel,
                              i == tab_ ? 2.0f : 1.0f, i == tab_ ? kAccent : kLine);
            ui::text(list, fonts.semibold, kTabNames[i], tab.cx(), 565.0f, 21.0f,
                     i == tab_ ? kInk : kMuted, gfx::Align::center, 2.0f);
        }
        static constexpr const char *kInventoryMessages[] = {
            "BASE GAME PACKAGE INVENTORY NOT PROVIDED",
            "DLC INVENTORY NOT PROVIDED",
            "TITLE UPDATE INVENTORY NOT PROVIDED",
        };
        list.bordered_rect({704.0f, 620.0f, 1016.0f, 168.0f}, 16.0f, kPanel, 1.0f, kLine);
        ui::text(list, fonts.semibold, kInventoryMessages[tab_], 736.0f, 670.0f,
                 22.0f, kAmber);
        ui::text(list, fonts.regular,
                 "ShadowMount reports titles and source paths; it does not list game, DLC, or update packages.",
                 736.0f, 716.0f, 19.0f, kMuted);
        ui::text(list, fonts.regular, "Install actions are unavailable until a supported installer is connected.",
                 704.0f, 862.0f, 20.0f, kAmber);
        ui::text(list, fonts.mono, game.path, 704.0f, 930.0f, 17.0f, kMuted);
    }

    void draw_detail(gfx::DrawList &list, const ui::Fonts &fonts) const
    {
        if (has_live_catalog_)
        {
            draw_live_detail(list, fonts);
            return;
        }
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
            list.bordered_rect(tab, 13.0f, i == tab_ ? kPanelLift : kPanel,
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
        list.bordered_rect({704.0f, 620.0f, 1016.0f, 168.0f}, 16.0f, kPanel, 1.0f, kLine);
        ui::text(list, fonts.regular, kind, 736.0f, 662.0f, 18.0f, kMuted,
                 gfx::Align::left, 2.0f);
        ui::text(list, fonts.mono, file, 736.0f, 706.0f, 21.0f, kInk);
        ui::text(list, fonts.mono, title.format, 736.0f, 752.0f, 21.0f, kAmber);
        ui::text(list, fonts.mono, tab_ == 0 ? title.size : "2.4 GB (sample)", 1668.0f, 752.0f,
                 21.0f, kInk, gfx::Align::right);
        list.bordered_rect({704.0f, 818.0f, 302.0f, 68.0f}, 15.0f, kPanelLift, 1.0f, kLine);
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
    std::string cache_path_;
    int focus_ = 0;
    int detail_ = -1;
    int tab_ = 0;
    int source_focus_ = 0;
    bool installed_only_ = false;
    bool blocks_ = false;
    bool source_open_ = false;
    float age_ = 0.0f;
    std::string notice_;
    std::string pending_json_;
    library::Catalog catalog_{};
    library::Catalog pending_catalog_{};
    library::FetchResult refresh_result_{};
    pthread_t refresh_thread_{};
    std::atomic<bool> refresh_done_{false};
    bool refresh_started_ = false;
    bool pending_cache_saved_ = false;
    bool catalog_from_cache_ = false;
    bool dependency_ready_ = false;
    bool has_live_catalog_ = false;
    int first_visible_ = 0;
};

} // namespace

std::unique_ptr<app::Concept> make_library(app::Context &context)
{
    return std::make_unique<Library>(context);
}

} // namespace hui::concepts

