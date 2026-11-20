#include "core/sketch_workflow.h"
#include "workspace.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>

using bitmap16::SketchWorkflow;
using Action = SketchWorkflow::Action;
using Choice = SketchWorkflow::Choice;
using Result = SketchWorkflow::Result;

namespace {
void require(bool value, const char* message) {
  if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
void environment(const char* name, const std::string& value) {
#ifdef _WIN32
  _putenv_s(name, value.c_str());
#else
  setenv(name, value.c_str(), 1);
#endif
}
struct Fixture {
  std::filesystem::path root;
  bitmap16::Editor editor;
  bitmap16::Desktop::Workspace workspace;
  bitmap16::Desktop::SketchStorage storage{workspace, editor};
  Fixture() {
    static int sequence = 0;
    root = std::filesystem::temp_directory_path() /
        ("bitmap16dx-sketch-workflow-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) +
         '-' + std::to_string(sequence++));
    environment("BITMAP16_DATA_DIR", root.string());
    environment("BITMAP16_CONFIG_DIR", "");
    require(workspace.initialize(editor), "Initialize isolated workspace");
  }
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
  std::string seed(uint8_t color) {
    require(storage.create(), "Create fixture sketch");
    editor.setSelectedColor(color);
    require(editor.draw() && storage.save(true), "Save fixture sketch");
    return storage.currentKey();
  }
  std::size_t index(const std::string& key) {
    for (std::size_t i = 0; i < workspace.sketches().size(); ++i)
      if (workspace.sketchKey(i) == key) return i;
    require(false, "Find sketch identity");
    return 0;
  }
  void edit() {
    editor.setCursor(1, 0);
    editor.setSelectedColor(3);
    require(editor.draw(), "Edit current sketch");
  }
};

void openKeepsItsTargetAcrossSaveReordering() {
  Fixture f;
  const auto first = f.seed(1);
  const auto second = f.seed(2);
  require(f.storage.open(first), "Open older current sketch");
  SketchWorkflow flow(f.editor, f.storage);
  f.edit();
  const auto selectedIndex = f.index(second);
  const auto target = f.workspace.sketchKey(selectedIndex);
  require(flow.request(Action::OpenSketch, true, target) == Result::AwaitingChoice,
          "Dirty Open requires a choice");
  require(flow.choose(Choice::Save) == Result::Completed, "Save then Open succeeds");
  require(f.index(second) != selectedIndex && f.storage.currentKey() == second &&
          f.editor.sketch().pixels[0][0] == 2 && f.editor.sketch().pixels[0][1] == 0,
          "Save promotion changes list order but Open keeps its selected identity");
  require(!f.editor.canUndo() && !flow.hasUnsavedChanges(), "Open establishes a clean history and baseline");

  f.edit();
  require(flow.request(Action::OpenSketch, true, second) == Result::AwaitingChoice &&
          flow.choose(Choice::Save) == Result::Completed, "Save then reopen current sketch");
  require(f.storage.currentKey() != second && f.editor.sketch().pixels[0][1] == 3,
          "Opening current sketch follows its promoted filename");
}

void failedSaveAndSaveAsKeepIdentityAndCanRetry() {
  Fixture f;
  const auto current = f.seed(1);
  const auto target = f.seed(2);
  require(f.storage.open(current), "Open fixture");
  SketchWorkflow flow(f.editor, f.storage);
  f.edit();
  const auto directory = f.root / "sketches";
  const auto parked = f.root / "parked-sketches";
  std::filesystem::rename(directory, parked);
  std::ofstream(directory) << "Block directory writes on every OS";
  require(!flow.save(true) && f.storage.currentKey() == current && flow.hasUnsavedChanges(),
          "Failed Save As keeps the current file identity and unsaved state");
  require(flow.request(Action::OpenSketch, true, target) == Result::AwaitingChoice &&
          flow.choose(Choice::Save) == Result::SaveFailed,
          "Failed Save blocks pending Open");
  require(flow.awaitingChoice() && f.storage.currentKey() == current &&
          f.editor.sketch().pixels[0][0] == 1 && f.editor.canUndo(),
          "Failed Save preserves the drawing, history and pending request");
  std::filesystem::remove(directory);
  std::filesystem::rename(parked, directory);
  require(flow.choose(Choice::Save) == Result::Completed && f.storage.currentKey() == target,
          "Retry completes the original Open exactly once");
}

void failedOpenAfterDiscardPreservesDrawingAndWarning() {
  Fixture f;
  const auto current = f.seed(1);
  const auto target = f.seed(2);
  require(f.storage.open(current), "Open fixture");
  SketchWorkflow flow(f.editor, f.storage);
  f.edit();
  require(flow.request(Action::OpenSketch, true, target) == Result::AwaitingChoice, "Prompt before Open");
  std::filesystem::remove(target);
  require(flow.choose(Choice::Discard) == Result::OpenFailed, "Missing file fails Open even with cached metadata");
  require(f.storage.currentKey() == current && f.editor.sketch().pixels[0][1] == 3 &&
          f.editor.canUndo() && flow.hasUnsavedChanges(),
          "Discard cannot clear the old drawing's warning when Open fails");
  require(flow.request(Action::Quit, true) == Result::AwaitingChoice, "Subsequent Quit still warns");
  require(flow.choose(Choice::Cancel) == Result::Cancelled, "Cancel leaves the drawing available");
}

void failedOpenAfterSaveKeepsThePromotedCurrentSketch() {
  Fixture f;
  const auto current = f.seed(1);
  const auto target = f.seed(2);
  require(f.storage.open(current), "Open fixture");
  SketchWorkflow flow(f.editor, f.storage);
  f.edit();
  require(flow.request(Action::OpenSketch, true, target) == Result::AwaitingChoice, "Prompt before Open");
  std::filesystem::remove(target);
  require(flow.choose(Choice::Save) == Result::OpenFailed, "Successful Save can be followed by failed Open");
  const auto promoted = f.storage.currentKey();
  require(promoted != current && std::filesystem::exists(promoted) &&
          f.workspace.sketchKey(f.index(promoted)) == promoted &&
          f.editor.sketch().pixels[0][1] == 3 && f.editor.canUndo() && !flow.hasUnsavedChanges(),
          "The saved drawing, promoted identity and refreshed catalog survive failed Open");
}

void deletedCurrentSketchStaysUnsavedUntilRestoreOrSave() {
  Fixture f;
  const auto current = f.seed(1);
  SketchWorkflow flow(f.editor, f.storage);
  require(f.workspace.deleteSketch(f.index(current)), "Delete current saved file");
  flow.savedCopyRemoved();
  require(f.workspace.hasDetachedSketch() && f.storage.currentKey().empty() && flow.hasUnsavedChanges(),
          "Losing the backing file is unsaved even if pixels equal the baseline");
  require(flow.request(Action::NewSketch, true) == Result::AwaitingChoice &&
          flow.choose(Choice::Cancel) == Result::Cancelled, "Backing loss participates in replacement warnings");
  require(f.workspace.undoDelete() && !f.workspace.hasDetachedSketch(), "Restore reattaches same current document");
  flow.savedCopyRestored();
  require(f.storage.currentKey() == current && !flow.hasUnsavedChanges(), "Restoration restores the saved state");
  require(f.workspace.deleteSketch(f.index(current)), "Delete current again");
  flow.savedCopyRemoved();
  require(flow.save() && !flow.hasUnsavedChanges() && !f.storage.currentKey().empty(),
          "Saving a detached drawing creates a new backing file");
}

void restoreCannotRebindADifferentDocument() {
  for (int operation = 0; operation < 3; ++operation) {
    Fixture f;
    const auto other = f.seed(2);
    const auto removed = f.seed(1);
    SketchWorkflow flow(f.editor, f.storage);
    require(f.workspace.deleteSketch(f.index(removed)), "Delete current file");
    flow.savedCopyRemoved();
    if (operation == 0) {
      flow.request(Action::NewSketch, false);
    } else if (operation == 1) {
      flow.request(Action::OpenSketch, false, other);
    } else {
      f.edit();
      require(flow.save(), "Save detached drawing under a new identity");
    }
    const auto key = f.storage.currentKey();
    const auto pixel = f.editor.sketch().pixels[0][0];
    const auto canUndo = f.editor.canUndo();
    require(f.workspace.undoDelete(), "Restore older deleted file");
    require(f.storage.currentKey() == key && f.editor.sketch().pixels[0][0] == pixel &&
            f.editor.canUndo() == canUndo,
            "Undo deletion cannot rebind the current sketch after New, Open or Save");
  }
}
}  // namespace

int main() {
  openKeepsItsTargetAcrossSaveReordering();
  failedSaveAndSaveAsKeepIdentityAndCanRetry();
  failedOpenAfterDiscardPreservesDrawingAndWarning();
  failedOpenAfterSaveKeepsThePromotedCurrentSketch();
  deletedCurrentSketchStaysUnsavedUntilRestoreOrSave();
  restoreCannotRebindADifferentDocument();
  std::cout << "6 sketch workflow persistence scenarios passed\n";
}
