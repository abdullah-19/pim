#pragma once

#include "components/ecs.h"
#include "threading/taskgraph.h"
#include <initializer_list>

struct IEntitySystem : TaskNode
{
    IEntitySystem(
        cstr name,
        std::initializer_list<cstr> edges,
        std::initializer_list<ComponentType> all,
        std::initializer_list<ComponentType> none);
    virtual ~IEntitySystem();

    void Execute(i32 begin, i32 end) final;
    void BeforeSubmit() final;
    virtual void Execute(Slice<const Entity> entities) = 0;

    static bool Overlaps(const IEntitySystem* pLhs, const IEntitySystem* pRhs);

private:
    Slice<const Entity> m_entities;
    Array<ComponentType> m_all;
    Array<ComponentType> m_none;
};
