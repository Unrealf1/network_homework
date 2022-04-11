#pragma once

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "game_object.hpp"


class Model {
public:
    using objects_t = std::vector<GameObject>;

    template<typename T>
    void add_object(T&& object) {
        m_objects.push_back(std::forward<T>(object));
    }

    const objects_t& get_objects() const {
        return m_objects;
    }
    
    objects_t& get_objects() {
        return m_objects;
    }

    GameObject& get_by_id(uint32_t id) {
        auto it = std::ranges::find_if(m_objects, [id](const auto& obj) { return obj.id == id; } );
        if (it == m_objects.end()) {
            throw std::out_of_range("no game object with given id");
        }
        return *it;
    }
private:
    objects_t m_objects;
};
