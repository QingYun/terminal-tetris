#pragma once
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

namespace details { class ComponentAccessor; }

class Component : public ComponentBase {
private:
  void present_(Canvas&, bool);
public:
  std::size_t node_type_;
  std::unique_ptr<ComponentBase> node_;
  friend class details::ComponentAccessor;
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
void renderComponent(ComponentBase *component, typename T::Props next_props) {
  T* target = dynamic_cast<T*>(component);
  target->componentWillUpdate(next_props);
  target->setProps(std::move(next_props));
  target->render();
}

template <typename T>
std::unique_ptr<ComponentBase> createComponent(typename T::Props next_props) {
  auto target = std::make_unique<T>(std::move(next_props));
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

template <typename T>
class ComponentHolderT : public ComponentHolder {
private:
  typename T::Props next_props_;

  void render_() override {
    if (!component_) {
      calculateNextProps(nullptr);
      component_ = createComponent<T>(std::move(next_props_));
      return;
    }
    if (next_props_) {
      renderComponent<T>(component_.get(), std::move(next_props_));
      return;
    }
  }
  
  void present_(Canvas& canvas, bool parent_updated) override {
    if (component_) component_->present(canvas, parent_updated);
  }

public:
  ComponentHolderT(std::string id, Updater updater)
  : ComponentHolder{id, typeid(T).hash_code(), std::move(updater)}, next_props_{TrivalConstruction_t{}} {}

  void setNextProps(typename T::Props next_props) {
    next_props_ = std::move(next_props);
  }
};

class ComponentAccessor {
private:
  Component *pc_;
public:
  ComponentAccessor(Component *pc);

  std::unique_ptr<ComponentBase>& getNode();
  void setNode(std::unique_ptr<ComponentBase> node);
  
  std::size_t getType();
  void setType(std::size_t node_type);
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

template <typename T>
class ComponentRenderer {
private:
  using Props = typename ComponentRendererHelper<T>::Props;

  ComponentRendererHelper<T> helper_;
  ComponentAccessor parent_;
  
public:
  ComponentRenderer(Component* parent, ChildrenCreator& children_creator, std::function<Props(Props)> props_updater) 
  : helper_{children_creator, props_updater}, parent_{parent} {}
  
  ~ComponentRenderer() {
    if (typeid(T).hash_code() != parent_.getType() || !parent_.getNode()) {
      // a different type component needed or no component existing at all
      parent_.setType(typeid(T).hash_code());
      parent_.setNode(createComponent<T>(helper_.updateProps(Props{})));
      return;
    }

    T* target = dynamic_cast<T*>(parent_.getNode().release());
    auto next_props = helper_.updateProps(target->getProps());
    if (next_props != target->getProps()) {
      renderComponent<T>(target, std::move(next_props));
    }
    parent_.setNode(std::unique_ptr<ComponentBase>{target});
  }
};

template <typename T>
class ChildRenderer {
private:
  using Props = typename ComponentRendererHelper<T>::Props;
  
  ChildrenCreator& children_creator_;
  std::function<Props(Props)> props_updater_;
  std::string id_;
  Children& next_children_;
  
public:
  ChildRenderer(std::string id, Children& next_children, 
                ChildrenCreator& children_creator, std::function<Props(Props)> props_updater)
  : children_creator_{children_creator}, props_updater_{props_updater}, 
    id_{id}, next_children_{next_children} {}
  
  ~ChildRenderer() {
    next_children_.emplace_front(new ComponentHolderT<T>{id_, 
      [c{std::move(children_creator_)}, u{std::move(props_updater_)}] 
      (ComponentBase *pc, ComponentHolder *ph) {
        ComponentRendererHelper<T> helper{c, std::ref(u)};
        auto component = dynamic_cast<T*>(pc);
        auto holder = dynamic_cast<ComponentHolderT<T>*>(ph);

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
    
#define PROPS(field) this->getProps().get<Props::Field::field>()

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
  details::ComponentRenderer<C> __DETAILS_COMPONENT_RENDERER_NAME(C) { \
    this, __DETAILS_CHILDREN_CREATOR_NAME(C), attr \
  }; \
  __DETAILS_CHILDREN_CREATOR_NAME(C) = [this] (bool *trivial_creator, Children *next_children)
  
#define CHILD_COMPONENT(C, id, attr) \
  if (*trivial_creator) { *trivial_creator = false; return; } \
  ChildrenCreator __DETAILS_CHILDREN_CREATOR_NAME(C); \
  details::ChildRenderer<C> __DETAILS_COMPONENT_RENDERER_NAME(C) { \
    id, *next_children, __DETAILS_CHILDREN_CREATOR_NAME(C), attr \
  }; \
  __DETAILS_CHILDREN_CREATOR_NAME(C) = [this] (bool *trivial_creator, Children *next_children)

#define NO_CHILDREN do { (void)trivial_creator; (void)next_children; } while(0);