#ifndef __bitvector_h
#define __bitvector_h

typedef unsigned bv_cell_t;
#define BV_CELL_SIZE 32

#define define_bitvector(name,length) bv_cell_t name[(length+BV_CELL_SIZE-1)/BV_CELL_SIZE];
#define bitvec_idx(n) ((n)/BV_CELL_SIZE)
#define bitvec_shift(n) ((n)&(BV_CELL_SIZE-1))
#define bitvec_mask(n) (1<<bitvec_shift(n))

static inline int bitvec_get (bv_cell_t *vector, int n)
{
    return (vector[bitvec_idx(n)] & bitvec_mask(n)) >> bitvec_shift(n);
}

static inline void bitvec_set (bv_cell_t *vector, int n, int value)
{
    if (value) vector[bitvec_idx(n)] |= bitvec_mask(n);
    else vector[bitvec_idx(n)] &= ~bitvec_mask(n);
}

#endif
