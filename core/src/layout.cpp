#include "pokedex/layout.h"

namespace pokedex {

Point centerOrigin(int dispW, int dispH, int imgW, int imgH) {
  return Point{(dispW - imgW) / 2, (dispH - imgH) / 2};
}

}  // namespace pokedex
