#pragma once

#include <RaidenUI/DOM/Element.hpp>

namespace RaidenUI {

// patches the live tree in-place to match the next tree
// after this call, live reflects next's structure/content,
// and next is in a valid-but-unspecified state
void reconcile(ElementNode *live, ElementNode *next);

} // namespace RaidenUI
