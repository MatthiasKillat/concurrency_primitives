#pragma once

#include <vector>
#include <optional>
#include <stdint.h>

#include "types.hpp"

namespace ws
{

//helper container, specific (underlying) type is not decided yet
//but it has to support efficient add, removal and iteration
//and (later) be implemented without dynamic memory

//TODO: size template arg
template <typename T>
class IndexedContainer
{
private:
    struct Node
    {
        template <typename... Args>
        Node(Args &&... args) : element(std::forward<Args>(args)...) {}

        bool isUsed{true};
        T element;
    };

public:
    IndexedContainer(size_t capacity) : m_capacity(capacity)
    {
        m_nodes.reserve(m_capacity);
    }

    template <typename... Args>
    std::optional<index_t> emplace(Args &&... args)
    {

        auto index = m_nodes.size();
        if (index < m_capacity)
        {
            //todo: just construct into aligned memory in all cases later
            //create a new node
            m_nodes.emplace_back(std::forward<Args>(args)...);
            ++m_size;
            return index;
        }

        if (size() < capacity())
        {
            //finding a free node with brute force is inefficient ...
            for (index = 0; index < capacity(); ++index)
            {
                auto &node = m_nodes[index];
                if (!node.isUsed)
                {
                    //alignment is ok, there was already a T before
                    new (&node.element) T(std::forward<Args>(args)...);
                    node.isUsed = true;
                    ++m_size;
                    return index;
                }
            }
        }

        return std::nullopt;
    }

    bool remove(index_t index)
    {
        auto &node = m_nodes[index];
        if (node.isUsed)
        {
            node.element.~T(); //keep the memory but destroy the content
            node.isUsed = false;
            --m_size;
            return true;
        }
        return false;
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

} // namespace ws
