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
#include "./action.h"

namespace details {

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

template <typename T> class ComponentAccessor;

// base class for all non-endpoint components
template <typename StoreT>
class Component : public ComponentBase {
protected:
  using StoreType = StoreT;
  StoreType& store_;
private:
  std::size_t node_type_;
  std::unique_ptr<ComponentBase> node_;

  void present_(Canvas& canvas, bool parent_updated) {
    // only components handle present, so just forward the call
    node_->present(canvas, parent_updated || updated_);
  }
public:
  Component(StoreT& store) : store_(store) {}
  friend class details::ComponentAccessor<StoreType>;
};

// base class for all endpoint components
template <typename T>
class EndComponent : public ComponentBase {
private:
  void render_() {
    // endpoint components do not render new components,
    // so just render children passed to it
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

// container for child component
// child components are only rendered when we really need it
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
    // actually create the component in the first rendering
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

// provide access to private members of a component
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

// handler of component property update
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

// set or update a component's render result
// should be only used in component.render()
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

// add child holders to a component rendered by ComponentRenderer
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

// similar to ChildRenderer, but only for forwarding existing
// child holders, which is in most cases props.children
template <typename VecT>
class ArrayRenderer {
private:
  Children& next_children_;
  VecT components_;

public:
  ArrayRenderer(Children& next_children, VecT components)
  : next_children_{next_children}, components_{std::move(components)} {}

  ~ArrayRenderer() {
    for (auto it = std::rbegin(components_); it != std::rend(components_); ++it) {
      next_children_.emplace_front(std::move(*it));
    }
  }
};

template <typename VecT>
ArrayRenderer<VecT> createArrayRender(Children& next_children, VecT&& components) {
  VecT copy_components{std::forward<VecT>(components)};
  return ArrayRenderer<VecT>{next_children, std::move(copy_components)};
}

}

using ComponentPointer = std::unique_ptr<details::ComponentBase>;

#define DECL_PROPS(fields) \
  public: \
    IMMUTABLE_STRUCT(Props, BOOST_PP_SEQ_PUSH_BACK(fields, (details::Children, children))); \
    const Props& getProps() const { return props_; } \
    void setProps(Props next_props) { props_ = ::std::move(next_props); } \
  private: \
    Props props_

#define PROPS(field) this->getProps().template get<Props::Field::field>()

#define __DETAILS_CHILDREN_CREATOR_NAME(C) BOOST_PP_CAT(__, BOOST_PP_CAT(C, BOOST_PP_CAT(__LINE__, CHILDREN_CREATOR__)))
#define __DETAILS_COMPONENT_RENDERER_NAME(C) BOOST_PP_CAT(__, BOOST_PP_CAT(C, BOOST_PP_CAT(__LINE__, COMPONENT_RENDERER__)))
#define __DETAILS_ARRAY_RENDERER_NAME BOOST_PP_CAT(__, BOOST_PP_CAT(__LINE__, ARRAY_RENDERER))

#define __DETAILS_PROPS_UPDATER_OP(s, prefix, tuple) \
  props.template update<prefix::BOOST_PP_TUPLE_ELEM(0, tuple)>(BOOST_PP_TUPLE_ELEM(1, tuple));
#define __DETAILS_PROPS_UPDATER(attr) [this] (auto props) { \
    BOOST_PP_SEQ_FOR_EACH(__DETAILS_PROPS_UPDATER_OP, decltype(props)::Field, attr) \
    return props; \
  }
#define __DETAILS_EMPTY_PROPS_UPDATER(attr) [] (auto props) { return props; }

#define ATTRIBUTES(attr) \
  BOOST_PP_IF(BOOST_PP_SEQ_SIZE(attr), __DETAILS_PROPS_UPDATER, __DETAILS_EMPTY_PROPS_UPDATER)(attr) \

#define RENDER_MAIN_COMPONENT(C, attr) \
  details::ChildrenCreator __DETAILS_CHILDREN_CREATOR_NAME(C); \
  details::ComponentRenderer<C<StoreT>, StoreT> __DETAILS_COMPONENT_RENDERER_NAME(C) { \
    this, __DETAILS_CHILDREN_CREATOR_NAME(C), attr \
  }; \
  __DETAILS_CHILDREN_CREATOR_NAME(C) = [this] (bool *trivial_creator, details::Children *next_children)

#define RENDER_CHILD_COMPONENT(C, id, attr) \
  if (*trivial_creator) { *trivial_creator = false; return; } \
  details::ChildrenCreator __DETAILS_CHILDREN_CREATOR_NAME(C); \
  details::ChildRenderer<C<StoreT>, StoreT> __DETAILS_COMPONENT_RENDERER_NAME(C) { \
    id, *next_children, __DETAILS_CHILDREN_CREATOR_NAME(C), attr, \
    details::ComponentAccessor<StoreT>{this}.getStore() \
  }; \
  __DETAILS_CHILDREN_CREATOR_NAME(C) = [this] (bool *trivial_creator, details::Children *next_children)

#define RENDER_COMPONENT_ARRAY(arr) \
  if (*trivial_creator) { *trivial_creator = false; return; } \
  auto __DETAILS_ARRAY_RENDERER_NAME = details::createArrayRender(*next_children, arr)

#define RENDER_COMPONENT(...) \
  BOOST_PP_IF(BOOST_PP_EQUAL(1, BOOST_PP_VARIADIC_SIZE(__VA_ARGS__)), RENDER_COMPONENT_ARRAY, \
  BOOST_PP_IF(BOOST_PP_EQUAL(2, BOOST_PP_VARIADIC_SIZE(__VA_ARGS__)), RENDER_MAIN_COMPONENT, \
  BOOST_PP_IF(BOOST_PP_EQUAL(3, BOOST_PP_VARIADIC_SIZE(__VA_ARGS__)), RENDER_CHILD_COMPONENT, \
  BOOST_PP_EMPTY)))(__VA_ARGS__)

#define NO_CHILDREN do { (void)trivial_creator; (void)next_children; } while(0);

#define CREATE_COMPONENT_CLASS(cname) \
  template <typename StoreT> class cname : public details::Component<StoreT>

#define CREATE_END_COMPONENT_CLASS(cname) \
  template <typename StoreT> class cname : public details::EndComponent<cname<StoreT>>

#define COMPONENT_WILL_MOUNT(cname) \
  cname(Props props, StoreT& store) : details::Component<StoreT>{store}, props_{std::move(props)} {} \
  void componentWillMount()

#define END_COMPONENT_WILL_MOUNT(cname) \
  cname(Props props, StoreT&) : props_{std::move(props)} {} \
  void componentWillMount()

#define DISPATCH(...) this->store_.template dispatch<ACTION(__VA_ARGS__)>