#include "pokedex/card_browser.h"

namespace pokedex {

BrowseAction CardBrowser::apply(Swipe swipe) {
  switch (swipe) {
    case Swipe::Left:
      if (index_ + 1 < count_) {
        ++index_;
        return BrowseAction::NextCard;
      }
      return BrowseAction::None;
    case Swipe::Right:
      if (index_ > 0) {
        --index_;
        return BrowseAction::PrevCard;
      }
      return BrowseAction::None;
    case Swipe::Up:
      return BrowseAction::GoHome;
    default:
      return BrowseAction::None;
  }
}

}  // namespace pokedex
