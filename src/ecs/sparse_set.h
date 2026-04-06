#pragma once

#include "ecs/entity.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace BilliardsSaloon
{
    template <typename Component>
    class SparseSet
    {
    public:
        static constexpr std::uint32_t INVALID_DENSE_INDEX = static_cast<std::uint32_t>(-1);

        [[nodiscard]] bool contains(Entity entity) const
        {
            if (entity.index >= m_sparse.size())
            {
                return false;
            }

            const std::uint32_t denseIndex = m_sparse[entity.index];
            if (denseIndex == INVALID_DENSE_INDEX)
            {
                return false;
            }

            return m_denseEntities[static_cast<std::size_t>(denseIndex)] == entity;
        }

        template <typename... Args> // Args of the Component ...
        Component& emplace(Entity entity, Args&&... args)
        {
            if (contains(entity))
            {
                throw std::logic_error("Component already exists on entity.");
            }

            ensureSparseCapacity(entity.index + 1U);

            const std::uint32_t denseIndex = static_cast<std::uint32_t>(m_components.size());

            m_sparse[entity.index] = denseIndex;
            m_denseEntities.push_back(entity);
            m_components.emplace_back(std::forward<Args>(args)...); // needed when, we will want to construct the component which is for now just a template

            return m_components.back();
        }

        void remove(Entity entity)
        {
            if (!contains(entity)) // doesn't exist
            {
                return;
            }

            const std::uint32_t denseIndex = m_sparse[entity.index];
            const std::uint32_t lastDenseIndex = static_cast<std::uint32_t>(m_components.size() - 1U);

            if (denseIndex != lastDenseIndex)
            {
                m_components[static_cast<std::size_t>(denseIndex)] =
                    std::move(m_components[static_cast<std::size_t>(lastDenseIndex)]);

                const Entity movedEntity = m_denseEntities[static_cast<std::size_t>(lastDenseIndex)];
                m_denseEntities[static_cast<std::size_t>(denseIndex)] = movedEntity;
                m_sparse[movedEntity.index] = denseIndex;
            }

            m_components.pop_back();
            m_denseEntities.pop_back();
            m_sparse[entity.index] = INVALID_DENSE_INDEX;
        }

        [[nodiscard]] Component* tryGet(Entity entity)
        {
            if (!contains(entity))
            {
                return nullptr;
            }

            const std::uint32_t denseIndex = m_sparse[entity.index];
            return &m_components[static_cast<std::size_t>(denseIndex)];
        }

        [[nodiscard]] const Component* tryGet(Entity entity) const
        {
            if (!contains(entity))
            {
                return nullptr;
            }

            const std::uint32_t denseIndex = m_sparse[entity.index];
            return &m_components[static_cast<std::size_t>(denseIndex)];
        }

        [[nodiscard]] Component& get(Entity entity)
        {
            Component* component = tryGet(entity);
            if (component == nullptr)
            {
                throw std::out_of_range("Component not found on entity.");
            }

            return *component;
        }

        [[nodiscard]] const Component& get(Entity entity) const
        {
            const Component* component = tryGet(entity);
            if (component == nullptr)
            {
                throw std::out_of_range("Component not found on entity.");
            }

            return *component;
        }

        [[nodiscard]] std::size_t size() const
        {
            return m_components.size();
        }

        [[nodiscard]] Entity entityAtDenseIndex(std::size_t index) const
        {
            return m_denseEntities[index];
        }

        [[nodiscard]] Component& componentAtDenseIndex(std::size_t index)
        {
            return m_components[index];
        }

        [[nodiscard]] const Component& componentAtDenseIndex(std::size_t index) const
        {
            return m_components[index];
        }

    private:
        void ensureSparseCapacity(std::uint32_t requiredSize)
        {
            if (requiredSize <= m_sparse.size())
            {
                return;
            }

            m_sparse.resize(requiredSize, INVALID_DENSE_INDEX);
        }

        std::vector<std::uint32_t> m_sparse; // -> index / another approach is to use an unordered_map here with entities as indices
        std::vector<Entity> m_denseEntities; // -> Entity
        std::vector<Component> m_components; // -> Component
    };
}