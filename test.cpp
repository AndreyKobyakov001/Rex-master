#include <iostream>
//happily stolen from ChatGPT. 
void testFunction() {
    int x = 0;        // Line 4 - ordinary assignment
    x = 3;            // Line 5 - ordinary reassignment
    int y = 1;        // Line 6 - useful later, not interesting
    int z = x;        // Line 7 - assignment on a variable (RHS not empty)
    int a = x * 3;    // Line 8 - variation on variable assignment
//    int b = x * y;    // Line 9 - variation on a variable assignment
//    int c = x * y * z // Line 10 - variation, more complex
}

int main() {
    testFunction();   
    return 0;         
}
