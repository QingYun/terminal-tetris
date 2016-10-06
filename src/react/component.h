#pragma once
#include <string>
#include <typeinfo>
#include <deque>
#include <vector>
#include <functional>
#include <algorithm>

#include "../utils/immutable-struct.hpp"

class ComponentBase {
public:
  virtual ~ComponentBase();
  virtual void render() = 0;
};

namespace details { class ComponentAccessor; }

class Component : public ComponentBase {
public:
  std::size_t node_type_;
  std::unique_ptr<Component> node_;
  friend class details::ComponentAccessor;
};

class EndComponent : public ComponentBase {};

namespace details {

class ComponentHolder;
using Child = std::shared_ptr<ComponentHolder>;
using Children = std::deque<Child>;
using ChildrenCreator = std::function<void(bool*, Children*)>;

template <typename T>
void renderComponent(Component* component, typename T::Props next_props) {
  T* target = dynamic_cast<T*>(component);
  target->componentWillUpdate(next_props);
  target->setProps(std::move(next_props));
  target->render();
}

template <typename T>
std::unique_ptr<Component> createComponent(typename T::Props next_props) {
  auto target = std::make_unique<T>(std::move(next_props));
  target->render();
  return std::unique_ptr<Component>(target.release());
}

class ComponentHolder : public ComponentBase {
public:
  // try to update component's props.
  // if there is any change made, store the new props in ComponentHolderT::next_props and return true
  // if no changes, return false
  using Updater = std::function<bool(Component*, ComponentHolder*)>;

private:
  // needs to be unique within, at least, same-level children
  const std::string id_;
  // comes from typeid(ComponentType).hash_code()
  std::size_t component_type_hash_;
  Updater updater_;

protected:
  std::unique_ptr<Component> component_;

public:
  ComponentHolder(std::string id, std::size_t type_hash, Updater updater);
  bool calculateNextProps(Component *component); 
  static Children mergeChildren(Children next_children, const Children& prev_children);
};

template <typename T>
class ComponentHolderT : public ComponentHolder {
private:
  typename T::Props next_props_;

public:
  ComponentHolderT(std::string id, Updater updater)
  : ComponentHolder{id, typeid(T).hash_code(), std::move(updater)}, next_props_{TrivalConstruction_t{}} {}

  void setNextProps(typename T::Props next_props) {
    next_props_ = std::move(next_props);
  }

  void render() override {
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
};

class ComponentAccessor {
private:
  Component *pc_;
public:
  ComponentAccessor(Component *pc);

  std::unique_ptr<Component>& getNode();
  void setNode(std::unique_ptr<Component> node);
  
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

    std::unique_ptr<T> target{dynamic_cast<T*>(parent_.getNode().release())};
    auto next_props = helper_.updateProps(target->getProps());
    if (next_props != target->getProps()) {
      renderComponent<T>(target.get(), std::move(next_props));
    }
    parent_.setNode(std::unique_ptr<Component>{target.release()});
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
      (Component *pc, ComponentHolder *ph) {
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

