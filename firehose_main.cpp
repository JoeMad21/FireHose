#include <omp.h>

int main() {
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            func1();
        }

        #pragma omp section
        {
            func2();
        }
    }

    return 0;  
}