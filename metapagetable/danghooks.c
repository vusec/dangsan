/* This function increases size of memory
 * allocation request by one byte to avoid
 * off-by-one legitimate out-of-bound.
 */ 
unsigned long
dang_alloc_size_hook(unsigned long size) {
    return (size + 1);
}
