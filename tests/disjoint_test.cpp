#include <gtest/gtest.h>

#include "rydz_ecs/access.hpp"
#include "rydz_ecs/query.hpp"
#include "rydz_ecs/system.hpp"

using namespace ecs;

namespace {
struct A { using T = Component; };
struct B { using T = Component; };
struct C { using T = Component; };
}

TEST(DisjointTest, WithWithoutIsDisjoint) {
    SystemAccess acc1;
    SystemParamTraits<Query<Mut<A>, Filters<Without<B>>>>::access(acc1);
    
    SystemAccess acc2;
    SystemParamTraits<Query<Mut<A>, Filters<With<B>>>>::access(acc2);

    EXPECT_TRUE(acc1.is_archetype_disjoint(acc2));
    EXPECT_TRUE(acc2.is_archetype_disjoint(acc1));
}

TEST(DisjointTest, OrFilterShouldNotConflateRequirements) {
    SystemAccess acc_or;
    SystemParamTraits<Query<Mut<A>, Filters<Or<With<B>, With<C>>>>>::access(acc_or);

    SystemAccess acc_without_b;
    SystemParamTraits<Query<Mut<A>, Filters<Without<B>>>>::access(acc_without_b);

    EXPECT_FALSE(acc_or.is_archetype_disjoint(acc_without_b));
}

TEST(DisjointTest, AddedFilterShouldBeDisjointWithWithout) {
    SystemAccess acc_added;
    SystemParamTraits<Query<Entity, Filters<Added<A>>>>::access(acc_added);

    SystemAccess acc_without;
    SystemParamTraits<Query<Entity, Filters<Without<A>>>>::access(acc_without);

    // Added implicitly requires A, so it's disjoint with Without<A>
    EXPECT_TRUE(acc_added.is_archetype_disjoint(acc_without));
}

TEST(DisjointTest, MutWithoutAddedShouldNotThrow) {
    SystemAccess acc1;
    SystemParamTraits<Query<Mut<A>>>::access(acc1);

    SystemAccess acc2;
    SystemParamTraits<Query<Mut<A>>>::access(acc2);

    EXPECT_FALSE(acc1.is_archetype_disjoint(acc2));
}

TEST(DisjointTest, OptShouldNotAddToArchetypeRequired) {
    SystemAccess acc_opt;
    SystemParamTraits<Query<Mut<A>, Opt<B>>>::access(acc_opt);

    SystemAccess acc_without;
    SystemParamTraits<Query<Mut<A>, Filters<Without<B>>>>::access(acc_without);

    // An entity with {A} (and no B) matches BOTH Q1 and Q2.
    // They both mutate A. 
    // If they are marked disjoint, we have a false negative, which is a bug.
    EXPECT_FALSE(acc_opt.is_archetype_disjoint(acc_without));
}
