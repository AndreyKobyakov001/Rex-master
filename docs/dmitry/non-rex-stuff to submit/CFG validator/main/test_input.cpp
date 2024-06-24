// This file provides a bunch of dataflows to test the validator.
// Each test case starts with "startX", where X is the ID of the test case. Each test case is under a namespace
// starting with "testY" where Y where Y is the ID of that namespace.
//
// Each test case mimics an isolated "callback" function with a single "message" parameter, and tests dataflows
// originating with this input parameter and ending with a variable called "end", mimicking a ROS publisher.
//
// Dataflows which are valid in the CFG are listed under "Positives", and invalid ones are listed under "False Positives"
//
// The test runner allows to select which testcases to run, or to run all of them (or run all under a given namespace).
// The selection is made via the number of the namespace or the test under the namespace (i.e., test1::start2 --> 1:2)

// Direct variable assignments
namespace test1 {
    // Positives: [x -> a -> end]
    void start1(int x) {
        int a, end;
        a = x;
        end = a;
    }
    // Positives: [x -> b -> end]
    // False positives: [x -> a -> end]
    void start2(int x) {
        int a, end;
        end = a;
        a = x;

        // The loop should make the previously invalid dataflow valid
        int b;
        while (true) {
            end = b;
            b = x;
        }
    }
    // False positives: [a -> b -> end]
    void start3(int a) {
        bool cond;
        int b;
        if (cond) {
            b = a;
        } else {
            int end = b;
        }
    }
    // Positives: [a -> b -> end]
    void start4(int a) {
        bool cond;
        int end, b;
        if (cond) {
            end = b;
        } else {
            b = a;
        }
        end = b;
    }
    // False Positives: [a -> b -> c -> end]
    void start5(int a) {
        bool cond1, cond2, cond3;
        int b, c, end;
        if (cond1) {
            b = a;
        } else if (cond2) {
            c = b;
        } else {
            end = c;
        }
    }
    // Looped version of above
    // Positives: [a -> b -> c -> end]
    void start6(int a) {
        bool cond1, cond2, cond3;
        int b, c, end;
        while (true) {
            if (cond1) {
                b = a;
            } else if (cond2) {
                c = b;
            } else {
                end = c;
            }
        }
    }
    // Positives: [a -> b -> c -> end]
    void start7(int a) {
        bool cond1, cond2, cond3;
        int b, c, end;
        if (cond1) {
            b = a;
            if (cond2) {
                c = b;
            }
        }
        if (cond3) {
            end = c;
        }
    }
}

// Function versions of the above tests
namespace test2 {
    void h(int p) {
        int end = p;
    }

    // Positives: [x -> a -> h::p -> end]
    void start1(int x) {
        int a;
        a = x;
        h(a);
    }
    // Positives: [x -> b -> h::p -> end]
    // False positives: [x -> a -> h::p -> end]
    void start2(int x) {
        int a;
        h(a);
        a = x;

        // The loop should make the previously invalid dataflow valid
        int b;
        while (true) {
            h(b);
            b = x;
        }
    }
    // False positives: [a -> b -> h::p -> end]
    void start3(int a) {
        bool cond;
        int b;
        if (cond) {
            b = a;
        } else {
            h(b);
        }
    }
    // Positives: [a -> b -> h::p -> end]
    void start4(int a) {
        bool cond;
        int b;
        if (cond) {
            h(b);
        } else {
            b = a;
        }
        h(b);
    }
    // False Positives: [a -> b -> c -> h::p -> end]
    void start5(int a) {
        bool cond1, cond2, cond3;
        int b, c;
        if (cond1) {
            b = a;
        } else if (cond2) {
            c = b;
        } else {
            h(c);
        }
    }
    // Looped version of above
    // Positives: [a -> b -> c -> h::p -> end]
    void start6(int a) {
        bool cond1, cond2, cond3;
        int b, c;
        while (true) {
            if (cond1) {
                b = a;
            } else if (cond2) {
                c = b;
            } else {
                h(c);
            }
        }
    }
    // Positives: [a -> b -> c -> h::p -> end]
    void start7(int a) {
        bool cond1, cond2, cond3;
        int b, c;
        if (cond1) {
            b = a;
            if (cond2) {
                c = b;
            }
        }
        if (cond3) {
            h(c);
        }
    }
}

// More functions
namespace test3 {
    int a, b, end;
    void first() {
        b = a;
    }
    void second() {
        end = b;
    }
    // Positives: [x -> a -> b -> end]
    void start1(int x) {
        a = x;
        first();
        second();
    }
    // False Positives: [x -> a -> b -> end]
    void start2(int x) {
        a = x;
        second();
        first();
    }


    // Wrap the consecutive calls within a single function
    void bar1() {
        first();
        second();
    }
    void bar2() {
        second();
        first();
    }
    // Positives: [x -> a -> b -> end]
    void start3(int x) {
        a = x;
        bar1();
    }
    // False Positives: [x -> a -> b -> end]
    void start4(int x) {
        a = x;
        bar2();
    }


    // Nest the pairs within even more functions
    void m2() { bar1(); }
    void m1() { m2(); }
    // Positives: [x -> a -> b -> end]
    void start5(int x) {
        a = x;
        m1();
    }
    void n2() { bar2(); }
    void n1() { n2(); }
    // False Positives: [x -> a -> b -> end]
    void start6(int x) {
        a = x;
        n1();
    }
}

// Testing that the correct function return is taken
namespace test4 {
    int b, end;
    void baz() {}
    void bar() {
        baz();
        end = b;
    }
    void start1(int a) {
        bar(); // if this isn't here, then the return from baz into bar won't be taken since returns are restricted to the call tree
        b = a;
        baz();
    }
}

// Testing the missing behavior as documented within one of validator.py's TODOs (this test case is included there as well)
namespace test5 {
    int a, b, end;
    void bar() {
        end = b;
        b = a;
    }
    void start1(int x) {
        a = x;
        bar();
        bar();
    }
}

// Full-blown complex path
// This is a very complex dataflow spanning multiple levels of function returns, callchains, local variables,
// and global variables. Unfortunately, the current implementation of the validator takes a very long time (~20s)
// to terminate on each test case. This can hopefully be improved. Fortunately, such complex dataflows
// haven't yet been discovered in autonomoose publish/subscribe chains.
namespace test6 {
    int g1, g2, g3, g4;

    void empty(int p) { }
    void f(int p) {
        g4 = p;
        empty(g4);
    }
    int e(int p) {
        int x = p;
        return x;
    }
    int d(int p) {
        return e(p);
    }
    void c(int p) {
        g1 = p;
    }
    void b(int p) {
        int x = p;
        c(x);
    }
    int a(int p) {
        b(p);
        g2 = g1;
        return d(g2);
    }
    // Positive: [start1::x -> a::p -> b::p -> b::x -> c::p -> g1 -> g2 -> d::p -> e::p -> e::x -> e::ret ->
    //            d::ret -> a::ret -> g3 -> f::p -> g4 -> start1::l -> start1::end]
    void start1(int x) {
        g3 = a(x);
        f(g3);
        int l = g4;
        int end = l;
    }

    int a2(int p) {
        g2 = g1; // destroy the previous dataflow by flipping this with the call to `b`
        b(p);
        return d(g2);
    }
    // False Positive: [start2::x -> a2::p -> b::p -> b::x -> c::p -> g1 -> g2 -> d::p -> e::p -> e::x -> e::ret ->
    //                  d::ret -> a2::ret -> g3 -> f::p -> g4 -> start2::l -> start2::end]
    void start2(int x) {
        g3 = a2(x);
        f(g3);
        int l = g4;
        int end = l;
    }

    // False Positive: [start3::x -> a::p -> b::p -> b::x -> c::p -> g1 -> g2 -> d::p -> e::p -> e::x -> e::ret ->
    //                  d::ret -> a::ret -> g3 -> f::p -> g4 -> start3::l -> start3::end]
    void start3(int x) {
        g3 = a(x);
        int l = g4; // destroy the previous dataflow by flipping this with the call to `f`
        f(g3);
        int end = l;
    }
}
