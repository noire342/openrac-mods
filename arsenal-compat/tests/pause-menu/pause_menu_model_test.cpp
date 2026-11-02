// SPDX-License-Identifier: GPL-3.0-or-later
#include "pause_menu_model.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <set>
#include <vector>

using namespace arsenal::pause;
namespace {
unsigned checks;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "pause model: %s\n", message); std::exit(1); }
}
bool same(const Snapshot& a, const Snapshot& b) { return a.revision == b.revision && a.focus == b.focus && a.visible == b.visible; }
std::vector<Entry> stock(bool goodies = false) { return {{1,10,true},{2,20,true},{3,30,true},{4,40,true},{5,50,true},{6,60,true},{7,70,goodies}}; }
void ring(const Snapshot& s, const std::vector<EntryId>& expected) {
    check(s.visible == expected, "visible entry order matches its complete composition");
    check(s.visible.empty() ? s.focus == 0 : s.contains(s.focus), "focus always belongs to the visible ring");
    check(std::set<EntryId>(s.visible.begin(), s.visible.end()).size() == s.visible.size(), "composition includes each identity exactly once");
    check(s.previous(0) == 0 && s.next(0) == 0 && s.previous(99999) == 0 && s.next(99999) == 0, "missing identities have no accidental neighbors");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto id = expected[i], before = expected[(i + expected.size() - 1) % expected.size()], after = expected[(i + 1) % expected.size()];
        check(s.previous(id) == before && s.next(id) == after, "both navigation directions include every visible item and wrap");
        check(s.next(s.previous(id)) == id && s.previous(s.next(id)) == id, "previous and next are reciprocal around the entire ring");
    }
}
}

int main() {
    {
        Model model;
        ring(model.snapshot(), {});
        check(model.set_focus(0) == Status::Ok && model.compose({}) == Status::Ok, "empty composition and focus are valid");
        check(model.register_entry({100,80,true}) == Status::Ok, "one extension needs no stock menu");
        ring(model.snapshot(), {100});
        check(model.remove_entry(100) == Status::Ok, "last extension can be unregistered");
        ring(model.snapshot(), {});
    }
    {
        Model model;
        check(model.compose(stock()) == Status::Ok, "compose stock with hidden Goodies");
        ring(model.snapshot(), {1,2,3,4,5,6});
        check(model.register_entry({100,80,true}) == Status::Ok, "register Componenti once");
        ring(model.snapshot(), {1,2,3,4,5,6,100});
        check(model.snapshot().previous(1) == 100 && model.snapshot().next(100) == 1, "Up from first reaches Componenti and Down returns to first");
        check(model.compose(stock(true)) == Status::Ok, "native Goodies can become visible on reopen");
        ring(model.snapshot(), {1,2,3,4,5,6,7,100});
        check(model.register_entry({200,90,true}) == Status::Ok && model.register_entry({150,55,true}) == Status::Ok, "multiple future entries compose by explicit order");
        ring(model.snapshot(), {1,2,3,4,5,150,6,7,100,200});
        check(model.compose(stock()) == Status::Ok, "hide Goodies while retaining registrations");
        ring(model.snapshot(), {1,2,3,4,5,150,6,100,200});
        check(model.set_focus(100) == Status::Ok, "focus identifies Componenti independently of index");
        const auto before = model.snapshot();
        for (unsigned i = 0; i < 30; ++i) check(model.compose(stock()) == Status::Ok, "repeated native opens accept the same stock description");
        check(same(before, model.snapshot()), "reopening a menu preserves focus and does not churn revisions");
        check(model.set_order(100,-10) == Status::Ok && model.snapshot().focus == 100, "moving an entry keeps its stable focus");
        ring(model.snapshot(), {100,1,2,3,4,5,150,6,200});
    }
    {
        Model model;
        check(model.compose(stock()) == Status::Ok && model.register_entry({100,80,true}) == Status::Ok && model.register_entry({200,90,true}) == Status::Ok, "focus transition fixture");
        check(model.set_focus(100) == Status::Ok && model.set_visibility(100,false) == Status::Ok && model.snapshot().focus == 200, "hiding focus selects its prior visible successor");
        check(model.set_visibility(100,true) == Status::Ok && model.snapshot().focus == 200, "restoring visibility does not steal focus");
        check(model.remove_entry(200) == Status::Ok && model.snapshot().focus == 1, "removing the focused last entry wraps focus to the first");
        check(model.set_focus(6) == Status::Ok, "stock focus can be tracked too");
        auto fewer = stock(); fewer[5].visible = false;
        check(model.compose(fewer) == Status::Ok && model.snapshot().focus == 100, "hiding a focused stock item selects the custom successor");
        check(model.compose({}) == Status::Ok && model.snapshot().focus == 100, "focus survives complete replacement of stock entries");
        check(model.remove_entry(100) == Status::Ok && model.snapshot().focus == 0, "empty composition clears focus");
    }
    {
        Model model;
        check(model.compose(stock()) == Status::Ok && model.register_entry({100,80,true}) == Status::Ok, "transaction rejection fixture");
        const auto before = model.snapshot();
        check(model.register_entry({100,12,false}) == Status::Duplicate && same(before,model.snapshot()), "duplicate registration cannot mutate the registered entry or focus");
        check(model.register_entry({7,80,true}) == Status::Duplicate && same(before,model.snapshot()), "hidden stock identities remain reserved");
        check(model.register_entry({0,80,true}) == Status::Invalid && same(before,model.snapshot()), "zero registration is rejected without a partial composition");
        auto duplicate = stock(); duplicate.push_back({1,90,false});
        check(model.compose(duplicate) == Status::Duplicate && same(before,model.snapshot()), "duplicate stock identity rejects the entire stock update, even hidden");
        auto conflict = stock(); conflict.push_back({100,90,true});
        check(model.compose(conflict) == Status::Duplicate && same(before,model.snapshot()), "stock/custom identity collision rejects complete composition atomically");
        auto invalid = stock(); invalid.push_back({0,90,false});
        check(model.compose(invalid) == Status::Invalid && same(before,model.snapshot()), "invalid hidden stock record leaves prior snapshot intact");
        check(model.remove_entry(999) == Status::NotFound && model.set_visibility(999,false) == Status::NotFound && model.set_order(999,0) == Status::NotFound && same(before,model.snapshot()), "missing-entry mutations cannot change state");
        check(model.set_focus(7) == Status::NotFound && model.set_focus(0) == Status::Invalid && same(before,model.snapshot()), "focus cannot point to hidden or missing entries");
        auto copy = model.snapshot(); copy.visible.clear(); copy.focus = 777;
        check(same(before, model.snapshot()), "snapshot values carry no mutable registry reference");
        check(model.compose(stock(true)) == Status::Ok, "valid stock update still succeeds after rejected updates");
        ring(model.snapshot(), {1,2,3,4,5,6,7,100});
    }
    {
        Snapshot empty, out;
        check(compose_snapshot({{2,0,true},{1,0,true}},{{200,0,true},{100,0,true}},empty,out) == Status::Ok, "pure composition accepts native equal-order stock entries");
        ring(out,{2,1,100,200});
        const auto retained = out;
        check(compose_snapshot({{1,0,true},{1,1,true}},{},out,out) == Status::Duplicate && same(out,retained), "aliased snapshot output stays unchanged when composition fails");
        check(compose_snapshot({{4,0,true},{3,0,true}},{},out,out) == Status::Ok && out.focus == 4, "when all previous entries disappear, focus selects new first entry");
        out.revision = std::numeric_limits<std::uint64_t>::max(); const auto full = out;
        check(compose_snapshot({{1,0,true}},{},out,out) == Status::Exhausted && same(full,out), "revision exhaustion cannot wrap identity or partially replace output");
    }
    {
        // Many future registrations, repeated native menu reconstruction,
        // visibility changes and removals: independent ring invariants.
        Model model;
        check(model.compose(stock()) == Status::Ok, "many-entry stock fixture");
        for (EntryId id = 100; id < 132; ++id) check(model.register_entry({id,std::int64_t(id),true}) == Status::Ok, "many future extensions register independently");
        for (unsigned pass = 0; pass < 60; ++pass) {
            const bool goodies = (pass & 1) != 0;
            check(model.compose(stock(goodies)) == Status::Ok, "recompose many entries with changing stock visibility");
            const EntryId toggled = 100 + pass % 32;
            check(model.set_visibility(toggled,(pass % 3) != 0) == Status::Ok, "custom visibility mutates independently of stock");
            const auto state = model.snapshot();
            check(state.visible.size() >= 6 && state.visible.size() <= 39, "composition keeps all stock and at most registered extensions");
            check(state.contains(7) == goodies && state.contains(state.focus), "hidden Goodies and stable focus remain consistent under repeated opens");
            std::set<EntryId> visited;
            auto at = state.visible.front();
            for (std::size_t step = 0; step < state.visible.size(); ++step) {
                check(visited.insert(at).second, "forward navigation visits every entry once");
                check(state.previous(state.next(at)) == at, "every forward link has the matching backward link");
                at = state.next(at);
            }
            check(at == state.visible.front() && visited.size() == state.visible.size(), "many-entry menu forms one complete circular ring");
        }
        for (EntryId id = 100; id < 132; ++id) check(model.remove_entry(id) == Status::Ok, "owner unload removes its registration cleanly");
        check(model.compose(stock()) == Status::Ok, "stock menu survives unloading every extension");
        ring(model.snapshot(),{1,2,3,4,5,6});
    }
    std::printf("pause menu model: %u checks passed\n", checks);
}
