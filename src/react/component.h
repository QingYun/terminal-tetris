#pragma once
#include <type_traits>
#include <string>
#include <typeinfo>
#include <deque>
#include <vector>
#include <functional>
#include <algorithm>

#include "../utils/immutable-struct.hpp"
#include "./canvas.h"

class ComponentBase {
private:
  virtual void render_() = 0;
  virtual void present_(Canvas&, bool) = 0;

protected:
  // need to redraw? set by render() and clear by present()
  bool updated_;

public:
  virtual ~ComponentBase();
  // generate / update child components to reflect the current props & state
  void render();
  // draw on canvas. Decide whether to actually draw something based on this->updated_ and parent_updated
  void present(Canvas& canvas, bool parent_updated);
};

namespace details { template <typename T> class ComponentAccessor; }

template <typename StoreT>
class Component : public ComponentBase {
public:
  using StoreType = StoreT;
private:
  StoreType& store_;
  std::size_t node_type_;
  std::unique_ptr<ComponentBase> node_;

  void present_(Canvas& canvas, bool parent_updated) {
    node_->present(canvas, parent_updated || updated_);
  }
public:
  Component(StoreT& store) : store_(store) {}
  friend class details::ComponentAccessor<StoreType>;
};

template <typename T>
class EndComponent : public ComponentBase {
private:
  void render_() {
    T* self = dynamic_cast<T*>(this);
    for (auto& pc : self->getProps().template get<T::Props::Field::children>()) {
      pc->render();
    }
  }

  void present_(Canvas& canvas, bool parent_updated) {
    T* self = dynamic_cast<T*>(this);
    bool should_redraw = parent_updated || updated_;

    // we are pure !
    if (should_redraw) {
      self->present(canvas);
    }

    for (auto& pc : self->getProps().template get<T::Props::Field::children>()) {
      pc->present(canvas, should_redraw);
    }
  }
};

namespace details {

class ComponentHolder;
using Child = std::shared_ptr<ComponentHolder>;
using Children = std::deque<Child>;
using ChildrenCreator = std::function<void(bool*, Children*)>;

template <typename T>
void renderComponent(ComponentBase& component, typename T::Props next_props) {
  T* target = dynamic_cast<T*>(&component);
  target->componentWillUpdate(next_props);
  target->setProps(std::move(next_props));
  target->render();
}

template <typename ChildT, typename StoreT>
std::unique_ptr<ComponentBase> createComponent(typename ChildT::Props next_props, StoreT& store) {
  auto target = std::make_unique<ChildT>(std::move(next_props), store);
  target->render();
  return std::unique_ptr<ComponentBase>(target.release());
}

class ComponentHolder : public ComponentBase {
public:
  // try to update component's props.
  // if there is any change made, store the new props in ComponentHolderT::next_props and return true
  // if no changes, return false
  using Updater = std::function<bool(ComponentBase*, ComponentHolder*)>;

private:
  // needs to be unique within, at least, same-level children
  const std::string id_;
  // comes from typeid(ComponentType).hash_code()
  std::size_t component_type_hash_;
  Updater updater_;

protected:
  std::unique_ptr<ComponentBase> component_;

public:
  ComponentHolder(std::string id, std::size_t type_hash, Updater updater);
  bool calculateNextProps(ComponentBase *component);
  static Children mergeChildren(Children next_children, const Children& prev_children);
};

template <typename ChildT, typename StoreT>
class ComponentHolderT : public ComponentHolder {
private:
  typename ChildT::Props next_props_;
  StoreT& store_;

  void render_() override {
    if (!component_) {
      calculateNextProps(nullptr);
      component_ = createComponent<ChildT>(std::move(next_props_), store_);
      return;
    }
    if (next_props_) {
      renderComponent<ChildT>(*component_.get(), std::move(next_props_));
      return;
    }
  }

  void present_(Canvas& canvas, bool parent_updated) override {
    if (component_) component_->present(canvas, parent_updated);
  }

public:
  ComponentHolderT(std::string id, StoreT& store, Updater updater)
  : ComponentHolder{id, typeid(ChildT).hash_code(), std::move(updater)}, next_props_{TrivalConstruction_t{}}, store_{store} {}

  void setNextProps(typename ChildT::Props next_props) {
    next_props_ = std::move(next_props);
  }
};

template <typename T>
class ComponentAccessor {
private:
  Component<T> *pc_;
public:
  ComponentAccessor(Component<T> *pc) : pc_{pc} {}

  std::unique_ptr<ComponentBase>& getNode() const { return pc_->node_; }
  void setNode(std::unique_ptr<ComponentBase> node) { pc_->node_ = std::move(node); }

  std::size_t getType() const { return pc_->node_type_; }
  void setType(std::size_t node_type) { pc_->node_type_ = node_type; }

  T& getStore() const { return pc_->store_; }
};

template <typename T>
class ComponentRendererHelper {
public:
  using Props = typename T::Props;

private:
  const ChildrenCreator& children_creator_;
  std::function<Props(Props)> props_updater_;

public:
  ComponentRendererHelper(const ChildrenCreator& children_creator, std::function<Props(Props)> props_updater)
  : children_creator_{children_creator}, props_updater_{props_updater} {}

  Props updateProps(Props prev_props) const {
    auto next_props = props_updater_(std::move(prev_props));

    bool trivial_creator = true;
    children_creator_(&trivial_creator, nullptr);
    if (!trivial_creator) {
      Children next_children;
      children_creator_(&trivial_creator, &next_children);
      next_props.template update<Props::Field::children>(
        ComponentHolder::mergeChildren(std::move(next_children), next_props.template get<Props::Field::children>())
      );
    }

    return next_props;
  }
};

template <typename ChildT, typename StoreT>
class ComponentRenderer {
private:
  using Props = typename ComponentRendererHelper<ChildT>::Props;

  ComponentRendererHelper<ChildT> helper_;
  ComponentAccessor<StoreT> parent_;

public:
  ComponentRenderer(Component<StoreT>* parent, ChildrenCreator& children_creator, std::function<Props(Props)> props_updater)
  : helper_{children_creator, props_updater}, parent_{parent} {}

  ~ComponentRenderer() {
    if (typeid(ChildT).hash_code() != parent_.getType() || !parent_.getNode()) {
      // a different type component needed or no component existing at all
      parent_.setType(typeid(ChildT).hash_code());
      parent_.setNode(createComponent<ChildT>(helper_.updateProps(Props{}), parent_.getStore()));
      return;
    }

    ChildT* target = dynamic_cast<ChildT*>(parent_.getNode().release());
    auto next_props = helper_.updateProps(target->getProps());
    if (next_props != target->getProps()) {
      renderComponent<ChildT>(*target, std::move(next_props));
    }
    parent_.setNode(std::unique_ptr<ComponentBase>{target});
  }
};

template <typename ChildT, typename StoreT>
class ChildRenderer {
private:
  using Props = typename ComponentRendererHelper<ChildT>::Props;

  ChildrenCreator& children_creator_;
  std::function<Props(Props)> props_updater_;
  std::string id_;
  Children& next_children_;
  StoreT& store_;

public:
  ChildRenderer(std::string id, Children& next_children, ChildrenCreator& children_creator,
                std::function<Props(Props)> props_updater, StoreT& store)
  : children_creator_{children_creator}, props_updater_{props_updater},
    id_{id}, next_children_{next_children}, store_{store} {}

  ~ChildRenderer() {
    next_children_.emplace_front(new ComponentHolderT<ChildT, StoreT>{id_, store_,
      [c{std::move(children_creator_)}, u{std::move(props_updater_)}]
      (ComponentBase *pc, ComponentHolder *ph) {
        ComponentRendererHelper<ChildT> helper{c, std::ref(u)};
        auto component = dynamic_cast<ChildT*>(pc);
        auto holder = dynamic_cast<ComponentHolderT<ChildT, StoreT>*>(ph);

        auto next_props = helper.updateProps(component ? component->getProps() : Props{});
        if (!component || next_props != component->getProps()) {
          holder->setNextProps(std::move(next_props));
          return true;
        }
        return false;
      }});
  }
};

}

#define DECL_PROPS(fields) \
  public: \
    IMMUTABLE_STRUCT(Props, BOOST_PP_SEQ_PUSH_BACK(fields, (Children, children))); \
    const Props& getProps() const { return props_; } \
    void setProps(Props next_props) { props_ = ::std::move(next_props); } \
  private: \
    Props props_

#define PROPS(field) this->getProps().template get<Props::Field::field>()

#define __DETAILS_CHILDREN_CREATOR_NAME(C) BOOST_PP_CAT(__, BOOST_PP_CAT(C, BOOST_PP_CAT(__LINE__, CHILDREN_CREATOR__)))
#define __DETAILS_COMPONENT_RENDERER_NAME(C) BOOST_PP_CAT(__, BOOST_PP_CAT(C, BOOST_PP_CAT(__LINE__, COMPONENT_RENDERER__)))

#define __DETAILS_PROPS_UPDATER_OP(s, prefix, tuple) \
  props.template update<prefix::BOOST_PP_TUPLE_ELEM(0, tuple)>(BOOST_PP_TUPLE_ELEM(1, tuple));
#define __DETAILS_PROPS_UPDATER(attr) [this] (auto props) { \
    BOOST_PP_SEQ_FOR_EACH(__DETAILS_PROPS_UPDATER_OP, decltype(props)::Field, attr) \
    return props; \
  }
#define __DETAILS_EMPTY_PROPS_UPDATER(attr) [] (auto props) { return props; }

#define ATTRIBUTES(attr) \
  BOOST_PP_IF(BOOST_PP_SEQ_SIZE(attr), __DETAILS_PROPS_UPDATER, __DETAILS_EMPTY_PROPS_UPDATER)(attr) \

#define COMPONENT(C, attr) \
  ChildrenCreator __DETAILS_CHILDREN_CREATOR_NAME(C); \
  details::ComponentRenderer<C<StoreT>, StoreT> __DETAILS_COMPONENT_RENDERER_NAME(C) { \
    this, __DETAILS_CHILDREN_CREATOR_NAME(C), attr \
  }; \
  __DETAILS_CHILDREN_CREATOR_NAME(C) = [this] (bool *trivial_creator, Children *next_children)

#define CHILD_COMPONENT(C, id, attr) \
  if (*trivial_creator) { *trivial_creator = false; return; } \
  ChildrenCreator __DETAILS_CHILDREN_CREATOR_NAME(C); \
  details::ChildRenderer<C<StoreT>, StoreT> __DETAILS_COMPONENT_RENDERER_NAME(C) { \
    id, *next_children, __DETAILS_CHILDREN_CREATOR_NAME(C), attr, \
    details::ComponentAccessor<StoreT>{this}.getStore() \
  }; \
  __DETAILS_CHILDREN_CREATOR_NAME(C) = [this] (bool *trivial_creator, Children *next_children)

#define NO_CHILDREN do { (void)trivial_creator; (void)next_children; } while(0);
