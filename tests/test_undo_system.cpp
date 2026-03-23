#include <catch2/catch_test_macros.hpp>
#include "core/UndoSystem.h"

using namespace FluentUI;

TEST_CASE("UndoStack basic execute and undo", "[undo]") {
    UndoStack stack;
    int value = 0;

    stack.Execute({
        "set to 10",
        [&]() { value = 10; },
        [&]() { value = 0; }
    });

    REQUIRE(value == 10);
    REQUIRE(stack.CanUndo());
    REQUIRE_FALSE(stack.CanRedo());

    stack.Undo();
    REQUIRE(value == 0);
    REQUIRE_FALSE(stack.CanUndo());
    REQUIRE(stack.CanRedo());
}

TEST_CASE("UndoStack redo", "[undo]") {
    UndoStack stack;
    int value = 0;

    stack.Execute({"set 5", [&]() { value = 5; }, [&]() { value = 0; }});
    stack.Undo();
    REQUIRE(value == 0);

    stack.Redo();
    REQUIRE(value == 5);
    REQUIRE_FALSE(stack.CanRedo());
}

TEST_CASE("UndoStack redo cleared on new execute", "[undo]") {
    UndoStack stack;
    int value = 0;

    stack.Execute({"a", [&]() { value = 1; }, [&]() { value = 0; }});
    stack.Execute({"b", [&]() { value = 2; }, [&]() { value = 1; }});
    stack.Undo();
    REQUIRE(value == 1);
    REQUIRE(stack.CanRedo());

    stack.Execute({"c", [&]() { value = 3; }, [&]() { value = 1; }});
    REQUIRE_FALSE(stack.CanRedo());
    REQUIRE(value == 3);
}

TEST_CASE("UndoStack multiple undo/redo", "[undo]") {
    UndoStack stack;
    int value = 0;

    stack.Execute({"1", [&]() { value = 1; }, [&]() { value = 0; }});
    stack.Execute({"2", [&]() { value = 2; }, [&]() { value = 1; }});
    stack.Execute({"3", [&]() { value = 3; }, [&]() { value = 2; }});

    REQUIRE(stack.Size() == 3);

    stack.Undo();
    REQUIRE(value == 2);
    stack.Undo();
    REQUIRE(value == 1);
    stack.Undo();
    REQUIRE(value == 0);
    REQUIRE_FALSE(stack.CanUndo());

    stack.Redo();
    stack.Redo();
    stack.Redo();
    REQUIRE(value == 3);
}

TEST_CASE("UndoStack descriptions", "[undo]") {
    UndoStack stack;
    int v = 0;

    REQUIRE(stack.UndoDescription().empty());
    REQUIRE(stack.RedoDescription().empty());

    stack.Execute({"first action", [&]() { v = 1; }, [&]() { v = 0; }});
    REQUIRE(stack.UndoDescription() == "first action");

    stack.Undo();
    REQUIRE(stack.RedoDescription() == "first action");
}

TEST_CASE("UndoStack Clear", "[undo]") {
    UndoStack stack;
    int v = 0;
    stack.Execute({"a", [&]() { v = 1; }, [&]() { v = 0; }});
    stack.Clear();
    REQUIRE(stack.Size() == 0);
    REQUIRE_FALSE(stack.CanUndo());
    REQUIRE_FALSE(stack.CanRedo());
}

TEST_CASE("UndoStack max size", "[undo]") {
    UndoStack stack;
    stack.SetMaxSize(3);
    int v = 0;

    for (int i = 1; i <= 5; ++i) {
        int prev = v;
        int next = i;
        stack.Execute({std::to_string(i), [&v, next]() { v = next; }, [&v, prev]() { v = prev; }});
    }

    REQUIRE(stack.Size() == 3);
}

TEST_CASE("UndoStack groups", "[undo]") {
    UndoStack stack;
    int a = 0, b = 0;

    stack.BeginGroup("group op");
    stack.AddToGroup({"set a", [&]() { a = 10; }, [&]() { a = 0; }});
    stack.AddToGroup({"set b", [&]() { b = 20; }, [&]() { b = 0; }});
    stack.EndGroup();

    REQUIRE(a == 10);
    REQUIRE(b == 20);
    REQUIRE(stack.Size() == 1);

    stack.Undo();
    REQUIRE(a == 0);
    REQUIRE(b == 0);
}

TEST_CASE("MakeValueCommand", "[undo]") {
    UndoStack stack;
    int target = 5;

    auto cmd = MakeValueCommand("change to 10", &target, 10);
    stack.Execute(std::move(cmd));
    REQUIRE(target == 10);

    stack.Undo();
    REQUIRE(target == 5);

    stack.Redo();
    REQUIRE(target == 10);
}
