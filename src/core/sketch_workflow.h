#pragma once

#include <string>

#include "core/editor.h"

namespace bitmap16 {

// Replacement policy is shared; adapters own files, sketch initialization and
// platform-specific cursor behavior. Keys are stable identities, not list indices.
class SketchWorkflow {
 public:
  enum class Action { None, NewSketch, OpenSketch, Quit };
  enum class Choice { Cancel = 0, Discard = 1, Save = 2 };
  enum class Result { Completed, AwaitingChoice, Cancelled, SaveFailed, OpenFailed, Busy };
  enum class DirtyPolicy { MatchSavedSketch, SinceLastSave };

  class Storage {
   public:
    virtual ~Storage() = default;
    virtual bool save(bool asNew) = 0;
    virtual bool create() = 0;
    virtual bool open(const std::string& key) = 0;
    virtual std::string currentKey() const = 0;
  };

  SketchWorkflow(Editor& editor, Storage& storage,
                 DirtyPolicy policy = DirtyPolicy::MatchSavedSketch);

  bool hasUnsavedChanges() const;
  void edited() { edited_ = true; }
  void savedCopyRemoved() { missingSavedCopy_ = true; }
  void savedCopyRestored() { missingSavedCopy_ = false; }
  bool save(bool asNew = false);

  Result request(Action action, bool warn, const std::string& key = {});
  bool awaitingChoice() const { return awaiting_; }
  Choice choice() const { return choice_; }
  void moveChoice(int delta);
  Result choose(Choice choice);

 private:
  void establishBaseline();
  void clearRequest();
  Result complete();

  Editor& editor_;
  Storage& storage_;
  DirtyPolicy policy_;
  Sketch saved_;
  bool edited_ = false;
  bool missingSavedCopy_ = false;
  Action pending_ = Action::None;
  std::string target_;
  Choice choice_ = Choice::Save;
  bool awaiting_ = false;
};

}  // namespace bitmap16
