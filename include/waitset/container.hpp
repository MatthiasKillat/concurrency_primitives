#pragma once

#include <vector>
#include <optional>
#include <stdint.h>

using index_t = size_t;

//helper container, specific (underlying) type is not decided yet
//but it has to support efficient add, removal and iteration
//and (later) be implemented without dynamic memory
template <typename T>
class Container
{
private:
    struct Node
    {
        template <typename... Args>
        Node(Args &&... args) : element(std::forward<Args>(args)...) {}

        bool used{true};
        T element;
    };

public:
    Container(size_t capacity) : m_capacity(capacity)
    {
        m_nodes.reserve(m_capacity);
    }

    template <typename... Args>
    std::optional<index_t> emplace(Args &&... args)
    {

        auto index = m_nodes.size();
        if (index < m_capacity)
        {
            //create a new node
            m_nodes.emplace_back(std::forward<Args>(args)...);
            ++m_size;
            return index;
        }

        if (size() < capacity())
        {
            //we can recycle a node
        }

        return std::nullopt;
    }

    T &operator[](index_t index)
    {
        return m_nodes[index].element;
    }

    size_t size()
    {
        return m_size;
    }

    size_t capacity()
    {
        return m_capacity;
    }

private:
    size_t m_capacity;
    size_t m_size{0};
    std::vector<Node> m_nodes;
};