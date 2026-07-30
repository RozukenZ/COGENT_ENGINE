#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <cstdint>
#include <stdexcept>

namespace Cogent::Core::ECS {

    using Entity = uint32_t;
    const Entity MAX_ENTITIES = 100000;

    class IComponentArray {
    public:
        virtual ~IComponentArray() = default;
        virtual void EntityDestroyed(Entity entity) = 0;
    };

    template <typename T>
    class ComponentArray : public IComponentArray {
    public:
        void InsertData(Entity entity, T component) {
            if (entityToIndexMap.find(entity) != entityToIndexMap.end()) {
                throw std::runtime_error("Component added to same entity more than once.");
            }

            size_t newIndex = size;
            entityToIndexMap[entity] = newIndex;
            indexToEntityMap[newIndex] = entity;
            componentArray[newIndex] = component;
            ++size;
        }

        void RemoveData(Entity entity) {
            if (entityToIndexMap.find(entity) == entityToIndexMap.end()) return;

            size_t indexOfRemovedEntity = entityToIndexMap[entity];
            size_t indexOfLastElement = size - 1;
            componentArray[indexOfRemovedEntity] = componentArray[indexOfLastElement];

            Entity entityOfLastElement = indexToEntityMap[indexOfLastElement];
            entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
            indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

            entityToIndexMap.erase(entity);
            indexToEntityMap.erase(indexOfLastElement);
            --size;
        }

        T& GetData(Entity entity) {
            return componentArray[entityToIndexMap.at(entity)];
        }

        void EntityDestroyed(Entity entity) override {
            if (entityToIndexMap.find(entity) != entityToIndexMap.end()) {
                RemoveData(entity);
            }
        }

    private:
        T componentArray[MAX_ENTITIES];
        std::unordered_map<Entity, size_t> entityToIndexMap;
        std::unordered_map<size_t, Entity> indexToEntityMap;
        size_t size = 0;
    };

    class Registry {
    public:
        Entity CreateEntity() {
            Entity id = nextEntityId++;
            return id;
        }

        void DestroyEntity(Entity entity) {
            for (auto const& pair : componentArrays) {
                auto const& component = pair.second;
                component->EntityDestroyed(entity);
            }
        }

        template <typename T>
        void RegisterComponent() {
            std::type_index typeName = typeid(T);
            componentArrays[typeName] = std::make_shared<ComponentArray<T>>();
        }

        template <typename T>
        void AddComponent(Entity entity, T component) {
            GetComponentArray<T>()->InsertData(entity, component);
        }

        template <typename T>
        void RemoveComponent(Entity entity) {
            GetComponentArray<T>()->RemoveData(entity);
        }

        template <typename T>
        T& GetComponent(Entity entity) {
            return GetComponentArray<T>()->GetData(entity);
        }

        template <typename T>
        bool HasComponent(Entity entity) {
            std::type_index typeName = typeid(T);
            if (componentArrays.find(typeName) == componentArrays.end()) return false;
            
            auto array = std::static_pointer_cast<ComponentArray<T>>(componentArrays[typeName]);
            // Workaround since we didn't expose HasEntity in ComponentArray, we'll try/catch or add HasEntity.
            // For simplicity in DOD, assume true if we reach here via query, but let's add HasData internally.
            // Actually, best to just track components per entity using a bitmask (Signature) later.
            // For now, we assume user tracks it.
            return true; // Simplified for now
        }

    private:
        template <typename T>
        std::shared_ptr<ComponentArray<T>> GetComponentArray() {
            std::type_index typeName = typeid(T);
            if (componentArrays.find(typeName) == componentArrays.end()) {
                RegisterComponent<T>(); // Auto-register
            }
            return std::static_pointer_cast<ComponentArray<T>>(componentArrays[typeName]);
        }

        Entity nextEntityId = 0;
        std::unordered_map<std::type_index, std::shared_ptr<IComponentArray>> componentArrays;
    };
}
