typedef char bool;

#define false 0
#define true 1

extern __thread bool malloc_flag;
extern __thread bool free_flag;

void *_Znam(unsigned long arg);
void *_Znwm(unsigned long arg);

void *PointerTrackerUninstrumented__Znam(unsigned long arg) {
    void *ptr;
    malloc_flag = true;
    ptr = _Znam(arg);
    malloc_flag = false;
    return ptr;
}

void *PointerTrackerUninstrumented__Znwm(unsigned long arg) {
    void *ptr;
    malloc_flag = true;
    ptr = _Znwm(arg);
    malloc_flag = false;
    return ptr;
}
