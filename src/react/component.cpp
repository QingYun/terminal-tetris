#include "./component.h"
#include "../utils/logger.h"

ComponentBase::~ComponentBase() {}

namespace details {

ComponentHolder::ComponentHolder(std::string id, std::size_t type_hash, Updater updater)
: id_(id), component_type_hash_(type_hash), updater_(updater) {}

bool ComponentHolder::calculateNextProps(ComponentBase *component) {
  return updater_(component, this);
}

Children ComponentHolder::mergeChildren(Children next_children, const Children& prev_children) {
  for (auto& pc : next_children) {
    auto prev_ppc = std::find_if(std::begin(prev_children), std::end(prev_children), [&pc] (const Child& prev_pc) {
      return pc->component_type_hash_ == prev_pc->component_type_hash_ && pc->id_ == prev_pc->id_;
    });

    if (prev_ppc == std::end(prev_children)) {
      // always assume an unrendered component is different from the new one
      // defer props calculation to the final render call as it's possible for a parent component 
      // to choose to not render some of its children
      continue;
    }

    // Note: to update, leave pc untouched; to keep, set pc = prev_pc
    const Child& prev_pc = *prev_ppc;
    if (pc == prev_pc) {
      // parent chooses to not update
      // skip children-forwarding in the middle of a component tree;
      continue;
    }

    if (!prev_pc->component_) {
      // update
      // no previous component, which means:
      // 1) never rendered or
      // 2) parent found it needs to be updated and has already moved it into pc
      //    Note: all children refers to same holder objects after rendering,
      //          i.e. sub-components do not create their own component holder,
      //          they just keep a reference to holders passed from parents.
      continue;
    }

    if (pc->calculateNextProps(prev_pc->component_.get())) {
      // update, property changed
      // Note: the next_props calculated in pc->calculateNextProps() will be memorized,
      //       so there will be no recalcuating when rendering
      pc->component_ = std::move(prev_pc->component_);
    } else {
      // do not update, no property changes
      // the next_props moved to the component during the last render
      // so no render will happen to this component again
      pc = prev_pc;
    }
  }
  return next_children;
}

ComponentAccessor::ComponentAccessor(Component *pc) : pc_{pc} {}

std::unique_ptr<ComponentBase>& ComponentAccessor::getNode() { return pc_->node_; }
void ComponentAccessor::setNode(std::unique_ptr<ComponentBase> node) { pc_->node_ = std::move(node); }

std::size_t ComponentAccessor::getType() { return pc_->node_type_; }
void ComponentAccessor::setType(std::size_t node_type) { pc_->node_type_ = node_type; }

}