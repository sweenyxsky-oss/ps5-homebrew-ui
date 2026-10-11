// ps5-homebrew-ui - Behaviour tests for the native library preview.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concept_fixture.hpp"
#include "concepts/concepts.hpp"

namespace
{

using hui::Action;
using hui::Direction;

class Library : public hui::testing::ConceptFixture
{
  protected:
    std::unique_ptr<hui::app::Concept> design_ = hui::concepts::make_library(context_);
};

TEST_F(Library, SourcePickerOpensAndReportsUnavailableBackend)
{
    send(*design_, press(Action::west));
    EXPECT_TRUE(played(hui::audio::Cue::open));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(hui::audio::Cue::error));
    expect_drawable(*design_);
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(hui::audio::Cue::back));
}

TEST_F(Library, TitleDetailsChangePackageTabsAndKeepInstallUnavailable)
{
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(hui::audio::Cue::select));
    send(*design_, nav(Direction::right));
    EXPECT_TRUE(played(hui::audio::Cue::focus));
    send(*design_, press(Action::confirm));
    EXPECT_TRUE(played(hui::audio::Cue::error));
    expect_drawable(*design_);
    send(*design_, press(Action::back));
    EXPECT_TRUE(played(hui::audio::Cue::back));
}

TEST_F(Library, LibraryModesAndLayoutsRemainNavigable)
{
    send(*design_, press(Action::menu));
    EXPECT_TRUE(played(hui::audio::Cue::toggle));
    send(*design_, press(Action::north));
    EXPECT_TRUE(played(hui::audio::Cue::toggle));
    send(*design_, nav(Direction::down));
    EXPECT_TRUE(played(hui::audio::Cue::focus));
    expect_drawable(*design_);
}

} // namespace
