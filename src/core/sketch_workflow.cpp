#include "core/sketch_workflow.h"

#include <algorithm>
#include <cstring>

namespace bitmap16 {

SketchWorkflow::SketchWorkflow(Editor& editor, Storage& storage, DirtyPolicy policy)
    : editor_(editor), storage_(storage), policy_(policy), saved_(editor.sketch()) {}

bool SketchWorkflow::hasUnsavedChanges() const {
  if (missingSavedCopy_) return true;
  if (policy_ == DirtyPolicy::SinceLastSave) return edited_;
  const auto& sketch = editor_.sketch();
  return sketch.gridSize != saved_.gridSize || sketch.paletteSize != saved_.paletteSize ||
      sketch.isEmpty != saved_.isEmpty ||
      std::memcmp(sketch.paletteColors, saved_.paletteColors, sizeof(saved_.paletteColors)) != 0 ||
      std::memcmp(sketch.pixels, saved_.pixels, sizeof(saved_.pixels)) != 0;
}

void SketchWorkflow::establishBaseline() {
  saved_ = editor_.sketch();
  edited_ = false;
  missingSavedCopy_ = false;
}

bool SketchWorkflow::save(bool asNew) {
  const std::string previous = storage_.currentKey();
  if (!storage_.save(asNew)) return false;
  // Saving promotes the file's name on both platforms. If Open refers to the
  // current sketch, follow that identity; every other target keeps its key.
  if (!asNew && pending_ == Action::OpenSketch && !previous.empty() && target_ == previous) {
    target_ = storage_.currentKey();
  }
  establishBaseline();
  return true;
}

void SketchWorkflow::clearRequest() {
  pending_ = Action::None;
  target_.clear();
  awaiting_ = false;
}

SketchWorkflow::Result SketchWorkflow::request(Action action, bool warn, const std::string& key) {
  if (awaiting_) return Result::Busy;
  pending_ = action;
  target_ = key;
  choice_ = Choice::Save;
  if (action == Action::None) {
    clearRequest();
    return Result::Cancelled;
  }
  if (warn && hasUnsavedChanges()) {
    awaiting_ = true;
    return Result::AwaitingChoice;
  }
  return complete();
}

void SketchWorkflow::moveChoice(int delta) {
  choice_ = static_cast<Choice>(std::max(0, std::min(2, static_cast<int>(choice_) + delta)));
}

SketchWorkflow::Result SketchWorkflow::choose(Choice choice) {
  if (!awaiting_) return Result::Cancelled;
  if (choice == Choice::Cancel) {
    clearRequest();
    return Result::Cancelled;
  }
  if (choice == Choice::Save && !save()) return Result::SaveFailed;
  // Discard authorizes replacement; it does not mark the old sketch clean.
  return complete();
}

SketchWorkflow::Result SketchWorkflow::complete() {
  const Action action = pending_;
  bool completed = false;
  if (action == Action::NewSketch) completed = storage_.create();
  else if (action == Action::OpenSketch) completed = storage_.open(target_);
  else if (action == Action::Quit) completed = true;
  clearRequest();
  if (!completed) return Result::OpenFailed;
  if (action != Action::Quit) establishBaseline();
  return Result::Completed;
}

}  // namespace bitmap16
