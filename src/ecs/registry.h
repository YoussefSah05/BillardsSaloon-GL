#pragma once

#include "ecs/entity.h"
#include "ecs/sparse_set.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace BilliardsSaloon
{
    class Registry
    {
    public:
        Entity createEntity()
        {
            std::uint32_t index = 0;

            if (!m_freeIndices.empty())
            {
                index = m_freeIndices.back();
                m_freeIndices.pop_back();
                m_alive[static_cast<std::size_t>(index)] = 1U;
            }
            else
            {
                index = static_cast<std::uint32_t>(m_generations.size());
                m_generations.push_back(0U);
                m_alive.push_back(1U);
            }

            return Entity{index, m_generations[static_cast<std::size_t>(index)]};
        }

        void destroyEntity(Entity entity)
        {
            if (!isAlive(entity))
            {
                return;
            }

            for (auto& [_, pool] : m_pools)
            {
                pool->remove(entity);
            }

            const std::size_t index = static_cast<std::size_t>(entity.index);
            m_alive[index] = 0U;
            ++m_generations[index];
            m_freeIndices.push_back(entity.index);
        }

        [[nodiscard]] bool isAlive(Entity entity) const
        {
            if (!entity.isValid())
            {
                return false;
            }

            if (entity.index >= m_generations.size())
            {
                return false;
            }

            const std::size_t index = static_cast<std::size_t>(entity.index);
            return (m_alive[index] != 0U) && (m_generations[index] == entity.generation);
        }

        template <typename Component, typename... Args>
        Component& emplace(Entity entity, Args&&... args)
        {
            validateAlive(entity);
            return getOrCreatePool<Component>().storage.emplace(entity, std::forward<Args>(args)...);
        }

        template <typename Component>
        [[nodiscard]] bool has(Entity entity) const
        {
            if (!isAlive(entity))
            {
                return false;
            }

            const Pool<Component>* pool = tryGetPool<Component>();
            return (pool != nullptr) && pool->storage.contains(entity);
        }

        template <typename Component>
        [[nodiscard]] Component& get(Entity entity)
        {
            validateAlive(entity);
            return getOrCreatePool<Component>().storage.get(entity);
        }

        template <typename Component>
        [[nodiscard]] const Component& get(Entity entity) const
        {
            validateAlive(entity);

            const Pool<Component>* pool = tryGetPool<Component>();
            if (pool == nullptr)
            {
                throw std::out_of_range("Component pool does not exist.");
            }

            return pool->storage.get(entity);
        }

        template <typename Component>
        [[nodiscard]] Component* tryGet(Entity entity)
        {
            if (!isAlive(entity))
            {
                return nullptr;
            }

            Pool<Component>* pool = tryGetPool<Component>();
            return (pool != nullptr) ? pool->storage.tryGet(entity) : nullptr;
        }

        template <typename Component>
        [[nodiscard]] const Component* tryGet(Entity entity) const
        {
            if (!isAlive(entity))
            {
                return nullptr;
            }

            const Pool<Component>* pool = tryGetPool<Component>();
            return (pool != nullptr) ? pool->storage.tryGet(entity) : nullptr;
        }

        template <typename Component>
        void remove(Entity entity)
        {
            if (!isAlive(entity))
            {
                return;
            }

            Pool<Component>* pool = tryGetPool<Component>();
            if (pool != nullptr)
            {
                pool->storage.remove(entity);
            }
        }

        template <typename... Components>
        class View
        {
        public:
            explicit View(Registry& registry)
                : m_registry(registry)
            {
            }

            template <typename Func>
            void each(Func&& func)
            {
                const IPool* leadPool = m_registry.selectLeadPool<Components...>();
                if (leadPool == nullptr)
                {
                    return;
                }

                const std::size_t count = leadPool->size();
                for (std::size_t i = 0; i < count; ++i)
                {
                    const Entity entity = leadPool->entityAtDenseIndex(i);

                    if ((m_registry.has<Components>(entity) && ...))
                    {
                        func(entity, m_registry.get<Components>(entity)...);
                    }
                }
            }

        private:
            Registry& m_registry;
        };

        template <typename... Components>
        [[nodiscard]] View<Components...> view()
        {
            return View<Components...>(*this);
        }

    private:
        struct IPool
        {
            virtual ~IPool() = default;

            virtual void remove(Entity entity) = 0;
            [[nodiscard]] virtual bool contains(Entity entity) const = 0;
            [[nodiscard]] virtual std::size_t size() const = 0;
            [[nodiscard]] virtual Entity entityAtDenseIndex(std::size_t index) const = 0;
        };

        template <typename Component>
        struct Pool final : IPool
        {
            SparseSet<Component> storage;

            void remove(Entity entity) override
            {
                storage.remove(entity);
            }

            [[nodiscard]] bool contains(Entity entity) const override
            {
                return storage.contains(entity);
            }

            [[nodiscard]] std::size_t size() const override
            {
                return storage.size();
            }

            [[nodiscard]] Entity entityAtDenseIndex(std::size_t index) const override
            {
                return storage.entityAtDenseIndex(index);
            }
        };

        void validateAlive(Entity entity) const
        {
            if (!isAlive(entity))
            {
                throw std::invalid_argument("Entity handle is not alive.");
            }
        }

        template <typename Component>
        Pool<Component>& getOrCreatePool()
        {
            const std::type_index key = std::type_index(typeid(Component));
            auto it = m_pools.find(key);

            if (it == m_pools.end())
            {
                auto pool = std::make_unique<Pool<Component>>();
                auto* raw = pool.get();
                m_pools.emplace(key, std::move(pool));
                return *raw;
            }

            return *static_cast<Pool<Component>*>(it->second.get());
        }

        template <typename Component>
        Pool<Component>* tryGetPool()
        {
            const std::type_index key = std::type_index(typeid(Component));
            auto it = m_pools.find(key);
            if (it == m_pools.end())
            {
                return nullptr;
            }

            return static_cast<Pool<Component>*>(it->second.get());
        }

        template <typename Component>
        const Pool<Component>* tryGetPool() const
        {
            const std::type_index key = std::type_index(typeid(Component));
            auto it = m_pools.find(key);
            if (it == m_pools.end())
            {
                return nullptr;
            }

            return static_cast<const Pool<Component>*>(it->second.get());
        }

        template <typename... Components>
        [[nodiscard]] const IPool* selectLeadPool() const
        {
            const IPool* best = nullptr;
            bool missingPool = false;

            auto consider = [&](const IPool* pool)
            {
                if (pool == nullptr)
                {
                    missingPool = true;
                    return;
                }

                if ((best == nullptr) || (pool->size() < best->size()))
                {
                    best = pool;
                }
            };

            (consider(tryGetPool<Components>()), ...);

            return missingPool ? nullptr : best;
        }

        std::vector<std::uint32_t> m_generations;
        std::vector<std::uint8_t> m_alive;
        std::vector<std::uint32_t> m_freeIndices;
        std::unordered_map<std::type_index, std::unique_ptr<IPool>> m_pools;
    };
}